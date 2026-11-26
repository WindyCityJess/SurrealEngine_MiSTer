// Drives the software render device without a window or game data: a floor in
// perspective with a lightmap, a masked sprite, a Gouraud triangle, a
// translucent and a modulated tile. Checks pixels whose values are known and
// writes the frame as a PPM. Also times the floor, for comparing builds.
// usage: SoftwareRenderTest [out.ppm] [width height]

#include "Precomp.h"
#include "RenderDevice/Software/SoftwareRenderDevice.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

static int failures = 0;

static void Check(const char* what, bool ok)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) failures++;
}

static int Red(uint32_t c) { return (c >> 16) & 255; }
static int Green(uint32_t c) { return (c >> 8) & 255; }
static int Blue(uint32_t c) { return c & 255; }

int main(int argc, char** argv)
{
	const char* out = argc > 1 ? argv[1] : "softwaretest.ppm";
	int width = argc > 3 ? atoi(argv[2]) : 320;
	int height = argc > 3 ? atoi(argv[3]) : 240;

	SoftwareRenderDevice device(nullptr);
	device.SetFrameSize(width, height);

	// A 64x64 checkerboard of 8x8 squares, white and red, with mipmaps
	TextureColor palette[256];
	for (int i = 0; i < 256; i++) palette[i] = TextureColor(0, 0, 0, 255);
	palette[1] = TextureColor(255, 255, 255, 255);
	palette[2] = TextureColor(255, 0, 0, 255);
	UnrealMipmap checkerMips[4];
	TextureInfo checker;
	for (int m = 0; m < 4; m++)
	{
		int size = 64 >> m;
		checkerMips[m].Width = size;
		checkerMips[m].Height = size;
		checkerMips[m].Data.resize(size * size);
		for (int y = 0; y < size; y++)
			for (int x = 0; x < size; x++)
				checkerMips[m].Data[y * size + x] = ((((x << m) / 8) + ((y << m) / 8)) & 1) ? 2 : 1;
	}
	checker.CacheID = 1;
	checker.Format = TextureFormat::P8;
	checker.USize = 64;
	checker.VSize = 64;
	checker.NumMips = 4;
	checker.Mips = checkerMips;
	checker.Palette = palette;

	// A 2x2 lightmap: full light on the left (127 is 1.0 of 7 bits, doubled by the device), dark on the right
	UnrealMipmap lightMip;
	lightMip.Width = 2;
	lightMip.Height = 2;
	lightMip.Data.resize(16);
	for (int i = 0; i < 4; i++)
	{
		uint8_t value = (i & 1) ? 16 : 64;
		lightMip.Data[i * 4] = value;
		lightMip.Data[i * 4 + 1] = value;
		lightMip.Data[i * 4 + 2] = value;
		lightMip.Data[i * 4 + 3] = 0;
	}
	TextureInfo light;
	light.CacheID = 2;
	light.Format = TextureFormat::BGRA8_LM;
	light.USize = 2;
	light.VSize = 2;
	light.UScale = 256.0f;
	light.VScale = 256.0f;
	light.NumMips = 1;
	light.Mips = &lightMip;

	// One white texel, and a masked 2x2 with one hole
	UnrealMipmap whiteMip;
	whiteMip.Width = 1;
	whiteMip.Height = 1;
	whiteMip.Data.resize(1);
	whiteMip.Data[0] = 1;
	TextureInfo white;
	white.CacheID = 3;
	white.Format = TextureFormat::P8;
	white.NumMips = 1;
	white.Mips = &whiteMip;
	white.Palette = palette;

	UnrealMipmap maskMip;
	maskMip.Width = 2;
	maskMip.Height = 2;
	maskMip.Data = { 0, 1, 1, 1 };
	TextureInfo mask;
	mask.CacheID = 4;
	mask.Format = TextureFormat::P8;
	mask.USize = 2;
	mask.VSize = 2;
	mask.NumMips = 1;
	mask.Mips = &maskMip;
	mask.Palette = palette;

	SceneNode frame;
	frame.XB = 0;
	frame.YB = 0;
	frame.X = width;
	frame.Y = height;
	frame.FX = (float)width;
	frame.FY = (float)height;
	frame.FX2 = width * 0.5f;
	frame.FY2 = height * 0.5f;
	frame.FovAngle = 90.0f;
	frame.ObjectToWorld = mat4::identity();
	frame.WorldToView = mat4::identity();
	frame.Projection = mat4::identity();

	// View space as the device sees it: x right, y down, z into the screen.
	// A floor 64 units below the eye, from z = 32 (clipped by the near plane
	// side of the screen) to z = 2048.
	vec3 floorPoints[4] = { vec3(-512.0f, 64.0f, 2048.0f), vec3(512.0f, 64.0f, 2048.0f), vec3(512.0f, 64.0f, 0.5f), vec3(-512.0f, 64.0f, 0.5f) };
	SurfaceFacet facet;
	facet.MapCoords.Origin = vec3(-256.0f, 64.0f, 0.0f);
	facet.MapCoords.XAxis = vec3(1.0f, 0.0f, 0.0f);
	facet.MapCoords.YAxis = vec3(0.0f, 0.0f, 1.0f);
	facet.MapCoords.ZAxis = vec3(0.0f, -1.0f, 0.0f);
	facet.Vertices = floorPoints;
	facet.VertexCount = 4;
	SurfaceInfo surface;
	surface.PolyFlags = 0;
	surface.Texture = &checker;
	surface.LightMap = &light;

	auto drawScene = [&]()
	{
		device.Lock(vec4(0.5f, 0.5f, 0.5f, 0.0f), vec4(0.0f), vec4(0.0f, 0.0f, 0.25f, 1.0f), nullptr, nullptr);
		device.SetSceneNode(&frame);
		device.DrawComplexSurface(surface, facet);
	};

	drawScene();
	const uint32_t* pixels = device.GetFrame();
	auto at = [&](int x, int y) { return pixels[y * width + x]; };

	// Above the horizon nothing was drawn: the clear colour
	Check("sky is the clear colour", at(width / 2, height / 4) == 0xff00003f);
	// Straight down the middle, three quarters down the screen: the floor at
	// depth z = 64 / tan(angle) where the row is half way to the bottom edge
	// (y/z = 0.5 * FY/FX... for 320x240 with 90 degrees: scale 160 pixels,
	// row 180 is 60.5 pixels below the centre, z = 64 * 160 / 60.5 = 169.3)
	uint32_t f = at(width / 2 - 2, height * 3 / 4);
	Check("floor is drawn below the horizon", f != 0xff00003f);
	Check("floor texel is white or red, lit (grey level between dark and full)", Red(f) > 30 && Red(f) <= 255 && (Green(f) == Blue(f)));
	// Lightmap: left of the screen is brighter than the right at the same row
	uint32_t l = at(width / 8, height - 4), r = at(width - width / 8, height - 4);
	Check("lightmap darkens to the right", Red(l) > Red(r));

	// Depth: a tile behind the floor (far) must not cover it, one in front must
	uint32_t before = at(width / 2, height - 10);
	device.DrawTile(white, width / 2 - 8.0f, height - 18.0f, 16.0f, 16.0f, 0, 0, 1, 1, 4000.0f, vec4(0.0f, 1.0f, 0.0f, 1.0f), vec4(0.0f), 0);
	Check("tile behind the floor is hidden", at(width / 2, height - 10) == before);
	device.DrawTile(white, width / 2 - 8.0f, height - 18.0f, 16.0f, 16.0f, 0, 0, 1, 1, 1.0f, vec4(0.0f, 1.0f, 0.0f, 1.0f), vec4(0.0f), 0);
	Check("tile in front of the floor is green", at(width / 2, height - 10) == 0xff00ff00);

	// Masked: the texel with palette index 0 leaves the frame as it was
	device.ClearZ();
	uint32_t skyBefore = at(11, 11);
	device.DrawTile(mask, 10.0f, 10.0f, 20.0f, 20.0f, 0, 0, 2, 2, 1.0f, vec4(1.0f), vec4(0.0f), PF_Masked);
	Check("masked texel is not drawn", at(11, 11) == skyBefore);
	Check("other texels of the masked tile are white", at(25, 25) == 0xffffffff);

	// Translucent: source plus destination times (1 - source)
	device.DrawTile(white, 40.0f, 10.0f, 20.0f, 20.0f, 0, 0, 1, 1, 1.0f, vec4(0.5f, 0.0f, 0.0f, 1.0f), vec4(0.0f), PF_Translucent);
	uint32_t t = at(50, 20);
	Check("translucent adds to the frame", Red(t) >= 126 && Red(t) <= 129 && Blue(t) == 0x3f);

	// Modulated: destination times source times two
	device.DrawTile(white, 25.0f, 25.0f, 4.0f, 4.0f, 0, 0, 1, 1, 1.0f, vec4(1.0f), vec4(0.0f), PF_Modulated);
	Check("modulated by white clamps to white", at(26, 26) == 0xffffffff);

	// Gouraud triangle with red, green and blue corners in front of everything
	GouraudVertex tri[3];
	tri[0].Point = vec3(-20.0f, -40.0f, 100.0f); tri[0].Light = vec3(1.0f, 0.0f, 0.0f);
	tri[1].Point = vec3(20.0f, -40.0f, 100.0f); tri[1].Light = vec3(0.0f, 1.0f, 0.0f);
	tri[2].Point = vec3(0.0f, -10.0f, 100.0f); tri[2].Light = vec3(0.0f, 0.0f, 1.0f);
	for (auto& p : tri) { p.UV = vec2(0.0f); p.Fog = vec4(0.0f); }
	device.DrawGouraudPolygon(white, tri, 3, 0);
	// centre of the triangle on screen: x = 160, y = 120 - 30 * 1.6 = 72
	uint32_t g = at(width / 2, height / 2 - (int)(30.0f * width / 200.0f));
	Check("gouraud triangle mixes its three corners", Red(g) > 40 && Green(g) > 40 && Blue(g) > 40 && Red(g) < 140);

	// A polygon wholly behind the near plane draws nothing
	GouraudVertex behind[3] = { tri[0], tri[1], tri[2] };
	for (auto& p : behind) p.Point.z = 0.25f;
	uint32_t centre = at(width / 2, height / 2);
	device.DrawGouraudPolygon(white, behind, 3, 0);
	Check("polygon behind the near plane is not drawn", at(width / 2, height / 2) == centre);

	device.EndFlash();
	device.Unlock(false);

	FILE* file = fopen(out, "wb");
	if (file)
	{
		fprintf(file, "P6\n%d %d\n255\n", width, height);
		for (int i = 0; i < width * height; i++)
		{
			uint8_t rgb[3] = { (uint8_t)Red(pixels[i]), (uint8_t)Green(pixels[i]), (uint8_t)Blue(pixels[i]) };
			fwrite(rgb, 1, 3, file);
		}
		fclose(file);
		printf("frame written to %s\n", out);
	}

	// Timing: the lit floor alone, half the screen, many times
	const int runs = 200;
	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < runs; i++)
		drawScene();
	double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / runs;
	printf("clear + lit floor (about half of %dx%d): %.3f ms a frame\n", width, height, ms);

	printf("%s\n", failures ? "FAILED" : "all checks passed");
	return failures ? 1 : 0;
}

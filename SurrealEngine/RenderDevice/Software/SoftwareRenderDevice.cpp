
#include "Precomp.h"
#include "SoftwareRenderDevice.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <surrealwidgets/core/widget.h>
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef MISTER_HYBRID
extern "C" {
#include "MiSTer/mister_port.h"
}
#endif

namespace
{
	const float NearZ = 1.0f; // the GPU devices' near plane
	const int SpanLength = 16; // pixels between two perspective divides

	inline int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
	inline bool IsPow2(int v) { return v > 0 && (v & (v - 1)) == 0; }

	// Bilinear sample of a lightmap or fogmap, clamped to its edges.
	// u and v are in texels with the texel centres at whole numbers.
	inline void SampleBilinear(const SoftwareRenderDevice::MipLevel& mip, float u, float v, int out[4])
	{
		int w = mip.width, h = mip.height;
		if (u < 0.0f) u = 0.0f; else if (u > (float)(w - 1)) u = (float)(w - 1);
		if (v < 0.0f) v = 0.0f; else if (v > (float)(h - 1)) v = (float)(h - 1);
		int iu = (int)u, iv = (int)v;
		int fu = (int)((u - (float)iu) * 256.0f);
		int fv = (int)((v - (float)iv) * 256.0f);
		int iu1 = iu + 1 < w ? iu + 1 : iu;
		int iv1 = iv + 1 < h ? iv + 1 : iv;
		const uint32_t* p = mip.pixels.data();
		uint32_t c00 = p[iv * w + iu], c10 = p[iv * w + iu1];
		uint32_t c01 = p[iv1 * w + iu], c11 = p[iv1 * w + iu1];
		for (int shift = 0, k = 2; shift <= 24; shift += 8, k--)
		{
			int a = (int)((c00 >> shift) & 255), b = (int)((c10 >> shift) & 255);
			int c = (int)((c01 >> shift) & 255), d = (int)((c11 >> shift) & 255);
			int top = a * 256 + (b - a) * fu;
			int bottom = c * 256 + (d - c) * fu;
			int value = (top * 256 + (bottom - top) * fv) >> 16;
			// shift 0: blue, 8: green, 16: red, 24: alpha
			out[shift == 24 ? 3 : k] = value;
		}
	}
}

SoftwareRenderDevice::SoftwareRenderDevice(Widget* viewport)
{
	Viewport = viewport;

	WhiteTexture = std::make_unique<CachedTexture>();
	WhiteTexture->mips.resize(1);
	WhiteTexture->mips[0].width = 1;
	WhiteTexture->mips[0].height = 1;
	WhiteTexture->mips[0].pixels.assign(1, 0xffffffff);
}

SoftwareRenderDevice::~SoftwareRenderDevice()
{
}

void SoftwareRenderDevice::SetFrameSize(int width, int height)
{
	if (width < 1) width = 1;
	if (height < 1) height = 1;
	if (width != Width || height != Height)
	{
		Width = width;
		Height = height;
		Color.assign((size_t)width * height, 0xff000000);
		Depth.assign((size_t)width * height, 0.0f);
	}
	SetClip(0, 0, Width, Height);
}

void SoftwareRenderDevice::SetClip(int x0, int y0, int x1, int y1)
{
	ClipX0 = ClampInt(x0, 0, Width);
	ClipY0 = ClampInt(y0, 0, Height);
	ClipX1 = ClampInt(x1, ClipX0, Width);
	ClipY1 = ClampInt(y1, ClipY0, Height);
}

void SoftwareRenderDevice::Flush(bool AllowPrecache)
{
	Textures[0].clear();
	Textures[1].clear();
}

bool SoftwareRenderDevice::Exec(std::string Cmd, OutputDevice& Ar)
{
	return false;
}

void SoftwareRenderDevice::Lock(vec4 InFlashScale, vec4 InFlashFog, vec4 ScreenClear, uint8_t* HitData, int* HitSize)
{
	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	if (Viewport)
		SetFrameSize(Viewport->GetNativePixelWidth(), Viewport->GetNativePixelHeight());

	uint32_t clear = 0xff000000 |
		((uint32_t)ClampInt((int)(ScreenClear.x * 255.0f), 0, 255) << 16) |
		((uint32_t)ClampInt((int)(ScreenClear.y * 255.0f), 0, 255) << 8) |
		(uint32_t)ClampInt((int)(ScreenClear.z * 255.0f), 0, 255);
	std::fill(Color.begin(), Color.end(), clear);
	std::fill(Depth.begin(), Depth.end(), 0.0f);

	if (HitSize)
		*HitSize = 0;

	IsLocked = true;
}

void SoftwareRenderDevice::Unlock(bool Blit)
{
	if (Blit && Viewport)
	{
		Viewport->PresentBitmap(Width, Height, Color.data());
#ifdef MISTER_HYBRID
		// one frame for every field of the core's picture
		MiSTer_WaitFrame();
#endif
	}
	IsLocked = false;
}

/////////////////////////////////////////////////////////////////////////////
// Textures

void SoftwareRenderDevice::ConvertMip(MipLevel& out, const UnrealMipmap& mip, TextureFormat format, const TextureColor* palette, bool masked)
{
	int w = mip.Width, h = mip.Height;
	size_t count = (size_t)w * h;
	out.width = w;
	out.height = h;
	out.pow2 = IsPow2(w) && IsPow2(h);
	out.pixels.resize(count);
	const uint8_t* src = mip.Data.data();
	uint32_t* dst = out.pixels.data();

	switch (format)
	{
	case TextureFormat::P8:
		if (!palette || mip.Data.size() < count)
		{
			std::fill(out.pixels.begin(), out.pixels.end(), 0xffffffff);
			break;
		}
		for (size_t i = 0; i < count; i++)
		{
			int idx = src[i];
			if (masked && idx == 0)
			{
				dst[i] = 0;
			}
			else
			{
				const TextureColor& c = palette[idx];
				uint32_t alpha = masked ? 255 : c.A;
				dst[i] = (alpha << 24) | ((uint32_t)c.R << 16) | ((uint32_t)c.G << 8) | c.B;
			}
		}
		break;

	case TextureFormat::BGRA8_LM:
		// 7 bits a channel: twice the value is the light (the GPU devices do the same)
		if (mip.Data.size() < count * 4)
		{
			std::fill(out.pixels.begin(), out.pixels.end(), 0xffffffff);
			break;
		}
		for (size_t i = 0; i < count; i++)
		{
			uint32_t b = std::min((uint32_t)src[i * 4] << 1, 255u);
			uint32_t g = std::min((uint32_t)src[i * 4 + 1] << 1, 255u);
			uint32_t r = std::min((uint32_t)src[i * 4 + 2] << 1, 255u);
			uint32_t a = std::min((uint32_t)src[i * 4 + 3] << 1, 255u);
			dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
		}
		break;

	case TextureFormat::BGRA8:
		if (mip.Data.size() < count * 4)
		{
			std::fill(out.pixels.begin(), out.pixels.end(), 0xffffffff);
			break;
		}
		for (size_t i = 0; i < count; i++)
			dst[i] = ((uint32_t)src[i * 4 + 3] << 24) | ((uint32_t)src[i * 4 + 2] << 16) | ((uint32_t)src[i * 4 + 1] << 8) | src[i * 4];
		break;

	case TextureFormat::RGB8:
		if (mip.Data.size() < count * 3)
		{
			std::fill(out.pixels.begin(), out.pixels.end(), 0xffffffff);
			break;
		}
		for (size_t i = 0; i < count; i++)
			dst[i] = 0xff000000 | ((uint32_t)src[i * 3] << 16) | ((uint32_t)src[i * 3 + 1] << 8) | src[i * 3 + 2];
		break;

	case TextureFormat::R5G6B5:
		if (mip.Data.size() < count * 2)
		{
			std::fill(out.pixels.begin(), out.pixels.end(), 0xffffffff);
			break;
		}
		for (size_t i = 0; i < count; i++)
		{
			uint32_t c = (uint32_t)src[i * 2] | ((uint32_t)src[i * 2 + 1] << 8);
			uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
			dst[i] = 0xff000000 | ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
		}
		break;

	default:
		std::fill(out.pixels.begin(), out.pixels.end(), 0xffffffff);
		break;
	}
}

bool SoftwareRenderDevice::SupportsTextureFormat(TextureFormat Format)
{
	switch (Format)
	{
	case TextureFormat::P8:
	case TextureFormat::BGRA8_LM:
	case TextureFormat::BGRA8:
	case TextureFormat::RGB8:
	case TextureFormat::R5G6B5:
		return true;
	default:
		return false;
	}
}

void SoftwareRenderDevice::UploadTexture(CachedTexture& tex, TextureInfo& info, bool masked)
{
	tex.mips.clear();
	for (int i = 0; i < info.NumMips; i++)
	{
		const UnrealMipmap& mip = info.Mips[i];
		if (mip.Width <= 0 || mip.Height <= 0 || mip.Data.empty())
			break;
		tex.mips.emplace_back();
		ConvertMip(tex.mips.back(), mip, info.Format, info.Palette, masked);
	}
	if (tex.mips.empty())
	{
		tex.mips.resize(1);
		tex.mips[0].width = 1;
		tex.mips[0].height = 1;
		tex.mips[0].pixels.assign(1, 0xffffffff);
	}
	info.bRealtimeChanged = false;
}

const SoftwareRenderDevice::CachedTexture* SoftwareRenderDevice::GetTexture(TextureInfo* info, bool masked)
{
	if (!info || info->NumMips < 1 || !info->Mips)
		return nullptr;

	if (info->Texture && (info->Texture->PolyFlags() & PF_Masked))
		masked = true;
	if (info->Format != TextureFormat::P8)
		masked = false;

	std::unique_ptr<CachedTexture>& tex = Textures[masked ? 1 : 0][info->CacheID];
	if (!tex)
	{
		tex = std::make_unique<CachedTexture>();
		UploadTexture(*tex, *info, masked);
	}
	else if (info->bRealtimeChanged)
	{
		UploadTexture(*tex, *info, masked);
	}
	return tex.get();
}

void SoftwareRenderDevice::PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags)
{
	GetTexture(&Info, !!(PolyFlags & PF_Masked));
}

void SoftwareRenderDevice::UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL)
{
	// the whole texture is converted again when it is next drawn
	Info.bRealtimeChanged = true;
}

/////////////////////////////////////////////////////////////////////////////
// Rasterizer

void SoftwareRenderDevice::DrawPolygon(const RVertex* v, int count, const DrawState& s)
{
	const CachedTexture* tex = s.tex ? s.tex : WhiteTexture.get();
	if (count < 3 || tex->mips.empty() || ClipX0 >= ClipX1 || ClipY0 >= ClipY1)
		return;

	const int n = s.numAttrs;

	// One set of gradients for the polygon, from the largest triangle of its fan
	int best = -1;
	float bestArea = 0.0f;
	for (int i = 1; i + 1 < count; i++)
	{
		float area = (v[i].x - v[0].x) * (v[i + 1].y - v[0].y) - (v[i + 1].x - v[0].x) * (v[i].y - v[0].y);
		if (std::fabs(area) > std::fabs(bestArea))
		{
			bestArea = area;
			best = i;
		}
	}
	if (best < 0 || std::fabs(bestArea) < 1.0e-4f)
		return;

	const RVertex& a = v[0];
	const RVertex& b = v[best];
	const RVertex& c = v[best + 1];
	float invArea = 1.0f / bestArea;
	float gradX[MaxAttrs], gradY[MaxAttrs];
	for (int k = 0; k < n; k++)
	{
		float d1 = b.attr[k] - a.attr[k];
		float d2 = c.attr[k] - a.attr[k];
		gradX[k] = (d1 * (c.y - a.y) - d2 * (b.y - a.y)) * invArea;
		gradY[k] = (d2 * (b.x - a.x) - d1 * (c.x - a.x)) * invArea;
	}

	float ymin = v[0].y, ymax = v[0].y;
	for (int i = 1; i < count; i++)
	{
		ymin = std::min(ymin, v[i].y);
		ymax = std::max(ymax, v[i].y);
	}
	if (!(ymax > (float)ClipY0) || !(ymin < (float)ClipY1))
		return;
	int y0 = std::max(ClipY0, (int)std::ceil(std::max(ymin, (float)ClipY0 - 1.0f) - 0.5f));
	int y1 = std::min(ClipY1, (int)std::ceil(std::min(ymax, (float)ClipY1 + 1.0f) - 0.5f));

	const MipLevel* lightmip = (s.light == LightSource::Lightmap && s.lightmap && !s.lightmap->mips.empty()) ? &s.lightmap->mips[0] : nullptr;
	const MipLevel* fogmip = (s.fogmap && !s.fogmap->mips.empty()) ? &s.fogmap->mips[0] : nullptr;
	const bool vertexLight = s.light == LightSource::Vertex;
	const bool hasFog = s.vertexFog || fogmip;
	const bool plainLight = !lightmip && !vertexLight && s.constLight[0] == 256 && s.constLight[1] == 256 && s.constLight[2] == 256;
	const int mipCount = (int)tex->mips.size();
	const bool texPow2 = tex->mips[0].pow2;
	const float texW = (float)tex->mips[0].width;
	const float texH = (float)tex->mips[0].height;
	const Blend blend = s.blend;
	const uint32_t blendR = (s.blendColor >> 16) & 255, blendG = (s.blendColor >> 8) & 255, blendB = s.blendColor & 255;

	// Light (256 = 1.0) and fog (red, green, blue 0..255, alpha 0..256) at one point of a span
	auto shade = [&](const float* P, int* light, int* fog)
	{
		if (lightmip)
		{
			int texel[4];
			SampleBilinear(*lightmip, P[3], P[4], texel);
			light[0] = texel[0] * 2;
			light[1] = texel[1] * 2;
			light[2] = texel[2] * 2;
		}
		else if (vertexLight)
		{
			light[0] = ClampInt((int)P[3], 0, 1024);
			light[1] = ClampInt((int)P[4], 0, 1024);
			light[2] = ClampInt((int)P[5], 0, 1024);
		}
		else
		{
			light[0] = s.constLight[0];
			light[1] = s.constLight[1];
			light[2] = s.constLight[2];
		}
		if (fogmip)
		{
			int texel[4];
			SampleBilinear(*fogmip, P[5], P[6], texel);
			fog[0] = texel[0];
			fog[1] = texel[1];
			fog[2] = texel[2];
			fog[3] = texel[3] + (texel[3] >> 7);
		}
		else if (s.vertexFog)
		{
			fog[0] = ClampInt((int)P[6], 0, 255);
			fog[1] = ClampInt((int)P[7], 0, 255);
			fog[2] = ClampInt((int)P[8], 0, 255);
			fog[3] = ClampInt((int)P[9], 0, 256);
		}
	};

	for (int y = y0; y < y1; y++)
	{
		float yc = (float)y + 0.5f;

		// Where the polygon's edges cross this row
		float xl = 1.0e30f, xr = -1.0e30f;
		for (int i = 0; i < count; i++)
		{
			const RVertex& p = v[i];
			const RVertex& q = v[i + 1 == count ? 0 : i + 1];
			float ya = p.y, yb = q.y;
			if (ya == yb)
				continue;
			if ((yc >= ya && yc < yb) || (yc >= yb && yc < ya))
			{
				float x = p.x + (yc - ya) * (q.x - p.x) / (yb - ya);
				xl = std::min(xl, x);
				xr = std::max(xr, x);
			}
		}
		if (!(xr > xl))
			continue;
		xl = std::max(xl, (float)ClipX0 - 1.0f);
		xr = std::min(xr, (float)ClipX1 + 1.0f);
		int x0 = std::max(ClipX0, (int)std::ceil(xl - 0.5f));
		int x1 = std::min(ClipX1, (int)std::ceil(xr - 0.5f));
		if (x0 >= x1)
			continue;

		float A[MaxAttrs];
		float px = (float)x0 + 0.5f - a.x;
		float py = yc - a.y;
		for (int k = 0; k < n; k++)
			A[k] = a.attr[k] + px * gradX[k] + py * gradY[k];

		float P0[MaxAttrs] = {}, P1[MaxAttrs] = {};
		int light0[3] = { 256, 256, 256 }, light1[3] = { 256, 256, 256 };
		int fog0[4] = {}, fog1[4] = {};

		{
			float z = 1.0f / std::max(A[0], 1.0e-7f);
			for (int k = 1; k < n; k++)
				P0[k] = A[k] * z;
			shade(P0, light0, fog0);
		}

		uint32_t* dst = Color.data() + (size_t)y * Width;
		float* zbuf = Depth.data() + (size_t)y * Width;

		int x = x0;
		while (x < x1)
		{
			int len = std::min(SpanLength, x1 - x);
			float flen = (float)len;

			float B[MaxAttrs];
			for (int k = 0; k < n; k++)
				B[k] = A[k] + gradX[k] * flen;
			{
				float z = 1.0f / std::max(B[0], 1.0e-7f);
				for (int k = 1; k < n; k++)
					P1[k] = B[k] * z;
				shade(P1, light1, fog1);
			}

			// Texture coordinates across the span, 16.16, kept near zero
			float fu0 = P0[1], fv0 = P0[2], fu1 = P1[1], fv1 = P1[2];
			if (texPow2)
			{
				float baseU = std::floor(fu0 / texW) * texW;
				float baseV = std::floor(fv0 / texH) * texH;
				fu0 -= baseU; fu1 -= baseU;
				fv0 -= baseV; fv1 -= baseV;
			}
			fu0 = std::min(std::max(fu0, -16000.0f), 16000.0f);
			fu1 = std::min(std::max(fu1, -16000.0f), 16000.0f);
			fv0 = std::min(std::max(fv0, -16000.0f), 16000.0f);
			fv1 = std::min(std::max(fv1, -16000.0f), 16000.0f);
			int u = (int)(fu0 * 65536.0f);
			int vv = (int)(fv0 * 65536.0f);
			int du = (int)((fu1 - fu0) * 65536.0f) / len;
			int dv = (int)((fv1 - fv0) * 65536.0f) / len;

			// The mipmap whose texels are about a pixel wide along the row
			int mip = 0;
			if (mipCount > 1)
			{
				int step = std::max(std::abs(du), std::abs(dv));
				while (mip + 1 < mipCount && (step >> mip) > 0x18000)
					mip++;
			}
			const MipLevel& level = tex->mips[mip];
			const uint32_t* texels = level.pixels.data();
			const int lw = level.width, lh = level.height;
			const int umask = lw - 1, vmask = lh - 1;
			const int shift = 16 + mip;

			int lr = light0[0] << 8, lg = light0[1] << 8, lb = light0[2] << 8;
			int dlr = ((light1[0] - light0[0]) << 8) / len;
			int dlg = ((light1[1] - light0[1]) << 8) / len;
			int dlb = ((light1[2] - light0[2]) << 8) / len;
			int fr = fog0[0] << 8, fg = fog0[1] << 8, fb = fog0[2] << 8, fa = fog0[3] << 8;
			int dfr = ((fog1[0] - fog0[0]) << 8) / len;
			int dfg = ((fog1[1] - fog0[1]) << 8) / len;
			int dfb = ((fog1[2] - fog0[2]) << 8) / len;
			int dfa = ((fog1[3] - fog0[3]) << 8) / len;

			float w = A[0];
			const float dw = gradX[0];

			for (int i = 0; i < len; i++, x++)
			{
				if (!s.depthTest || w >= zbuf[x])
				{
					uint32_t texel;
					if (texPow2)
					{
						texel = texels[((vv >> shift) & vmask) * lw + ((u >> shift) & umask)];
					}
					else
					{
						int tu = ClampInt(u >> shift, 0, umask);
						int tv = ClampInt(vv >> shift, 0, vmask);
						texel = texels[tv * lw + tu];
					}

					uint32_t alpha = texel >> 24;
					if (!s.masked || alpha >= 128)
					{
						int r = (int)((texel >> 16) & 255);
						int g = (int)((texel >> 8) & 255);
						int bl = (int)(texel & 255);

						if (!plainLight)
						{
							r = (r * (lr >> 8)) >> 8; if (r > 255) r = 255;
							g = (g * (lg >> 8)) >> 8; if (g > 255) g = 255;
							bl = (bl * (lb >> 8)) >> 8; if (bl > 255) bl = 255;
						}
						if (hasFog)
						{
							int keep = 256 - (fa >> 8);
							r = (fr >> 8) + ((r * keep) >> 8); if (r > 255) r = 255;
							g = (fg >> 8) + ((g * keep) >> 8); if (g > 255) g = 255;
							bl = (fb >> 8) + ((bl * keep) >> 8); if (bl > 255) bl = 255;
						}

						switch (blend)
						{
						case Blend::Opaque:
							break;
						case Blend::Translucent:
						{
							uint32_t d = dst[x];
							int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
							r = r + dr - ((r * dr) >> 8);
							g = g + dg - ((g * dg) >> 8);
							bl = bl + db - ((bl * db) >> 8);
							break;
						}
						case Blend::Modulated:
						{
							uint32_t d = dst[x];
							int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
							r = (r * dr) >> 7; if (r > 255) r = 255;
							g = (g * dg) >> 7; if (g > 255) g = 255;
							bl = (bl * db) >> 7; if (bl > 255) bl = 255;
							break;
						}
						case Blend::Highlighted:
						{
							uint32_t d = dst[x];
							int keep = 255 - (int)alpha;
							keep += keep >> 7;
							r += (int)(((d >> 16) & 255) * keep) >> 8; if (r > 255) r = 255;
							g += (int)(((d >> 8) & 255) * keep) >> 8; if (g > 255) g = 255;
							bl += (int)((d & 255) * keep) >> 8; if (bl > 255) bl = 255;
							break;
						}
						case Blend::SubpixelFont:
						{
							// each channel of the glyph is the coverage of that subpixel
							uint32_t d = dst[x];
							int cr = r + (r >> 7), cg = g + (g >> 7), cb = bl + (bl >> 7);
							r = (int)((blendR * cr + ((d >> 16) & 255) * (256 - cr)) >> 8);
							g = (int)((blendG * cg + ((d >> 8) & 255) * (256 - cg)) >> 8);
							bl = (int)((blendB * cb + (d & 255) * (256 - cb)) >> 8);
							break;
						}
						}

						dst[x] = 0xff000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
						if (s.depthWrite)
							zbuf[x] = w;
					}
				}

				u += du; vv += dv;
				w += dw;
				lr += dlr; lg += dlg; lb += dlb;
				fr += dfr; fg += dfg; fb += dfb; fa += dfa;
			}

			for (int k = 0; k < n; k++)
				A[k] = B[k];
			for (int k = 1; k < n; k++)
				P0[k] = P1[k];
			for (int k = 0; k < 3; k++)
				light0[k] = light1[k];
			for (int k = 0; k < 4; k++)
				fog0[k] = fog1[k];
		}
	}
}

// Clips a convex polygon in view space ([0..2]: position, then its attributes)
// against the near plane
int SoftwareRenderDevice::ClipNear(const float (*in)[MaxAttrs + 3], int count, float (*out)[MaxAttrs + 3], int numAttrs) const
{
	int total = 3 + numAttrs - 1;
	int outCount = 0;
	for (int i = 0; i < count; i++)
	{
		const float* p = in[i];
		const float* q = in[i + 1 == count ? 0 : i + 1];
		bool pin = p[2] >= NearZ;
		bool qin = q[2] >= NearZ;
		if (pin)
		{
			if (outCount < MaxVerts)
				memcpy(out[outCount++], p, sizeof(float) * total);
		}
		if (pin != qin)
		{
			float t = (NearZ - p[2]) / (q[2] - p[2]);
			if (outCount < MaxVerts)
			{
				float* o = out[outCount++];
				for (int k = 0; k < total; k++)
					o[k] = p[k] + (q[k] - p[k]) * t;
				o[2] = NearZ;
			}
		}
	}
	return outCount;
}

void SoftwareRenderDevice::DrawViewPolygon(const float (*verts)[MaxAttrs + 3], int count, const DrawState& state, bool fan)
{
	static thread_local float clipped[MaxVerts][MaxAttrs + 3];
	static thread_local RVertex projected[MaxVerts];

	count = ClipNear(verts, count, clipped, state.numAttrs);
	if (count < 3)
		return;

	for (int i = 0; i < count; i++)
	{
		const float* p = clipped[i];
		float w = 1.0f / p[2];
		RVertex& r = projected[i];
		r.x = CenterX + p[0] * w * ScaleX;
		r.y = CenterY + p[1] * w * ScaleY;
		r.attr[0] = w;
		for (int k = 1; k < state.numAttrs; k++)
			r.attr[k] = p[2 + k] * w;
	}

	if (!fan || count == 3)
	{
		DrawPolygon(projected, count, state);
	}
	else
	{
		// attributes that are not planar over the polygon: a triangle at a time
		for (int i = 1; i + 1 < count; i++)
		{
			RVertex tri[3] = { projected[0], projected[i], projected[i + 1] };
			DrawPolygon(tri, 3, state);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Draw calls

uint32_t SoftwareRenderDevice::ApplyPrecedenceRules(uint32_t PolyFlags) const
{
	if (!(PolyFlags & (PF_Translucent | PF_Modulated)))
		PolyFlags |= PF_Occlude;
	else if (PolyFlags & PF_Translucent)
		PolyFlags &= ~PF_Masked;
	return PolyFlags;
}

void SoftwareRenderDevice::SetBlendState(DrawState& state, uint32_t PolyFlags) const
{
	if (PolyFlags & PF_Translucent)
		state.blend = Blend::Translucent;
	else if (PolyFlags & PF_Modulated)
		state.blend = Blend::Modulated;
	else if (PolyFlags & PF_Highlighted)
		state.blend = Blend::Highlighted;
	else
		state.blend = Blend::Opaque;
	state.depthWrite = !!(PolyFlags & PF_Occlude);
	state.masked = !!(PolyFlags & PF_Masked);
}

vec3 SoftwareRenderDevice::ToView(const vec3& p) const
{
	vec4 r = ObjectToView * vec4(p.x, p.y, p.z, 1.0f);
	return vec3(r.x, r.y, r.z);
}

void SoftwareRenderDevice::SetSceneNode(SceneNode* Frame)
{
	CurrentFrame = Frame;
	ObjectToView = Frame->WorldToView * Frame->ObjectToWorld;

	float RProjZ = (float)std::tan(radians(Frame->FovAngle) * 0.5);
	ScaleX = Frame->FX / (2.0f * RProjZ);
	ScaleY = ScaleX;
	CenterX = (float)Frame->XB + Frame->FX2;
	CenterY = (float)Frame->YB + Frame->FY2;
	SetClip(Frame->XB, Frame->YB, Frame->XB + Frame->X, Frame->YB + Frame->Y);
}

void SoftwareRenderDevice::DrawComplexSurface(SurfaceInfo& Surface, SurfaceFacet& Facet)
{
	uint32_t PolyFlags = ApplyPrecedenceRules(Surface.PolyFlags);
	if ((PolyFlags & PF_Invisible) || !CurrentFrame || !Facet.Vertices || Facet.VertexCount < 3)
		return;

	DrawState state;
	SetBlendState(state, PolyFlags);
	state.tex = GetTexture(Surface.Texture, state.masked);
	state.lightmap = GetTexture(Surface.LightMap, false);
	if (Surface.FogMap && Surface.FogMap->NumMips > 0 && Surface.FogMap->Mips && !Surface.FogMap->Mips[0].Data.empty())
		state.fogmap = GetTexture(Surface.FogMap, false);
	state.light = state.lightmap ? LightSource::Lightmap : LightSource::Constant;
	state.numAttrs = state.fogmap ? 7 : (state.lightmap ? 5 : 3);

	float UDot = dot(Facet.MapCoords.XAxis, Facet.MapCoords.Origin);
	float VDot = dot(Facet.MapCoords.YAxis, Facet.MapCoords.Origin);

	// to texels of each texture's first mipmap
	float UPan = 0.0f, VPan = 0.0f, UScale = 0.0f, VScale = 0.0f;
	if (state.tex)
	{
		UPan = UDot + Surface.Texture->Pan.x;
		VPan = VDot + Surface.Texture->Pan.y;
		UScale = 1.0f / Surface.Texture->UScale;
		VScale = 1.0f / Surface.Texture->VScale;
	}
	float LMUPan = 0.0f, LMVPan = 0.0f, LMUScale = 0.0f, LMVScale = 0.0f;
	if (state.lightmap)
	{
		LMUPan = UDot + Surface.LightMap->Pan.x - 0.5f * Surface.LightMap->UScale;
		LMVPan = VDot + Surface.LightMap->Pan.y - 0.5f * Surface.LightMap->VScale;
		LMUScale = 1.0f / Surface.LightMap->UScale;
		LMVScale = 1.0f / Surface.LightMap->VScale;
	}
	float FogUPan = 0.0f, FogVPan = 0.0f, FogUScale = 0.0f, FogVScale = 0.0f;
	if (state.fogmap)
	{
		FogUPan = UDot + Surface.FogMap->Pan.x - 0.5f * Surface.FogMap->UScale;
		FogVPan = VDot + Surface.FogMap->Pan.y - 0.5f * Surface.FogMap->VScale;
		FogUScale = 1.0f / Surface.FogMap->UScale;
		FogVScale = 1.0f / Surface.FogMap->VScale;
	}

	static thread_local float verts[MaxVerts][MaxAttrs + 3];
	int count = (int)std::min(Facet.VertexCount, (uint32_t)MaxVerts);
	for (int i = 0; i < count; i++)
	{
		const vec3& point = Facet.Vertices[i];
		float u = dot(Facet.MapCoords.XAxis, point);
		float v = dot(Facet.MapCoords.YAxis, point);
		vec3 view = ToView(point);
		float* o = verts[i];
		o[0] = view.x;
		o[1] = view.y;
		o[2] = view.z;
		o[3] = (u - UPan) * UScale;
		o[4] = (v - VPan) * VScale;
		// bilinear samples: texel centres at whole numbers
		o[5] = (u - LMUPan) * LMUScale - 0.5f;
		o[6] = (v - LMVPan) * LMVScale - 0.5f;
		o[7] = (u - FogUPan) * FogUScale - 0.5f;
		o[8] = (v - FogVPan) * FogVScale - 0.5f;
	}

	DrawViewPolygon(verts, count, state, false);
}

void SoftwareRenderDevice::DrawGouraudPolygon(TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags)
{
	if (NumPts < 3 || !CurrentFrame)
		return;
	PolyFlags = ApplyPrecedenceRules(PolyFlags);
	if (PolyFlags & PF_Invisible)
		return;

	DrawState state;
	SetBlendState(state, PolyFlags);
	state.tex = GetTexture(&Info, state.masked);
	state.vertexFog = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog;
	bool modulated = !!(PolyFlags & PF_Modulated);
	state.light = modulated ? LightSource::Constant : LightSource::Vertex;
	state.numAttrs = state.vertexFog ? 10 : (modulated ? 3 : 6);

	float UScale = 1.0f / Info.UScale;
	float VScale = 1.0f / Info.VScale;

	static thread_local float verts[MaxVerts][MaxAttrs + 3];
	int count = std::min(NumPts, (int)MaxVerts);
	for (int i = 0; i < count; i++)
	{
		const GouraudVertex& P = Pts[i];
		vec3 view = ToView(P.Point);
		float* o = verts[i];
		o[0] = view.x;
		o[1] = view.y;
		o[2] = view.z;
		o[3] = P.UV.x * UScale;
		o[4] = P.UV.y * VScale;
		o[5] = P.Light.x * 256.0f;
		o[6] = P.Light.y * 256.0f;
		o[7] = P.Light.z * 256.0f;
		o[8] = P.Fog.x * 255.0f;
		o[9] = P.Fog.y * 255.0f;
		o[10] = P.Fog.z * 255.0f;
		o[11] = P.Fog.w * 256.0f;
	}

	DrawViewPolygon(verts, count, state, true);
}

void SoftwareRenderDevice::DrawTile(TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 InColor, vec4 Fog, uint32_t PolyFlags)
{
	if (!CurrentFrame)
		return;

	if ((PolyFlags & PF_Modulated) && Info.Format == TextureFormat::P8)
		PolyFlags = PF_Modulated;

	// PF_SubpixelFont is every bit set: a value of its own, not a flag
	bool subpixel = PolyFlags == PF_SubpixelFont;
	PolyFlags = subpixel ? 0 : ApplyPrecedenceRules(PolyFlags);
	if (PolyFlags & PF_Invisible)
		return;

	DrawState state;
	SetBlendState(state, PolyFlags);
	state.tex = GetTexture(&Info, state.masked);
	state.numAttrs = 3;
	if (subpixel)
	{
		state.blend = Blend::SubpixelFont;
		state.masked = false;
		state.depthWrite = false;
		state.blendColor = 0xff000000 |
			((uint32_t)ClampInt((int)(InColor.x * 255.0f), 0, 255) << 16) |
			((uint32_t)ClampInt((int)(InColor.y * 255.0f), 0, 255) << 8) |
			(uint32_t)ClampInt((int)(InColor.z * 255.0f), 0, 255);
	}
	else if (!(PolyFlags & PF_Modulated))
	{
		state.constLight[0] = ClampInt((int)(InColor.x * 256.0f), 0, 1024);
		state.constLight[1] = ClampInt((int)(InColor.y * 256.0f), 0, 1024);
		state.constLight[2] = ClampInt((int)(InColor.z * 256.0f), 0, 1024);
	}

	float UScale = state.tex ? 1.0f / Info.UScale : 0.0f;
	float VScale = state.tex ? 1.0f / Info.VScale : 0.0f;
	float u0 = U * UScale, v0 = V * VScale;
	float u1 = (U + UL) * UScale, v1 = (V + VL) * VScale;
	float w = Z > 0.0f ? 1.0f / Z : 1.0f;
	float x0 = (float)CurrentFrame->XB + X, y0 = (float)CurrentFrame->YB + Y;
	float x1 = x0 + XL, y1 = y0 + YL;

	RVertex quad[4];
	quad[0] = { x0, y0, { w, u0 * w, v0 * w } };
	quad[1] = { x1, y0, { w, u1 * w, v0 * w } };
	quad[2] = { x1, y1, { w, u1 * w, v1 * w } };
	quad[3] = { x0, y1, { w, u0 * w, v1 * w } };
	DrawPolygon(quad, 4, state);
}

void SoftwareRenderDevice::DrawStencilPolygon(StencilPolygonType type, const vec3* Pts, int NumPts)
{
	// No stencil buffer yet: portals and mirrors are drawn without their mask
}

void SoftwareRenderDevice::SetStencilRef(int reference)
{
}

bool SoftwareRenderDevice::ProjectLine(vec3 p1, vec3 p2, float& x1, float& y1, float& w1, float& x2, float& y2, float& w2) const
{
	if (p1.z < NearZ && p2.z < NearZ)
		return false;
	if (p1.z < NearZ)
	{
		float t = (NearZ - p1.z) / (p2.z - p1.z);
		p1 = vec3(p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t, NearZ);
	}
	else if (p2.z < NearZ)
	{
		float t = (NearZ - p2.z) / (p1.z - p2.z);
		p2 = vec3(p2.x + (p1.x - p2.x) * t, p2.y + (p1.y - p2.y) * t, NearZ);
	}
	w1 = 1.0f / p1.z;
	w2 = 1.0f / p2.z;
	x1 = CenterX + p1.x * w1 * ScaleX;
	y1 = CenterY + p1.y * w1 * ScaleY;
	x2 = CenterX + p2.x * w2 * ScaleX;
	y2 = CenterY + p2.y * w2 * ScaleY;
	return true;
}

void SoftwareRenderDevice::DrawLineScreen(float x1, float y1, float w1, float x2, float y2, float w2, uint32_t color, bool depthTest)
{
	float dx = x2 - x1, dy = y2 - y1;
	float length = std::max(std::fabs(dx), std::fabs(dy));
	if (!(length < 100000.0f))
		return;
	int steps = std::max((int)std::ceil(length), 1);
	float sx = dx / (float)steps, sy = dy / (float)steps, sw = (w2 - w1) / (float)steps;
	for (int i = 0; i <= steps; i++)
	{
		int px = (int)std::floor(x1 + sx * (float)i);
		int py = (int)std::floor(y1 + sy * (float)i);
		if (px < ClipX0 || px >= ClipX1 || py < ClipY0 || py >= ClipY1)
			continue;
		size_t offset = (size_t)py * Width + px;
		if (depthTest && w1 + sw * (float)i < Depth[offset])
			continue;
		Color[offset] = color;
	}
}

static uint32_t ToColor(const vec4& c)
{
	return 0xff000000 |
		((uint32_t)ClampInt((int)(c.x * 255.0f), 0, 255) << 16) |
		((uint32_t)ClampInt((int)(c.y * 255.0f), 0, 255) << 8) |
		(uint32_t)ClampInt((int)(c.z * 255.0f), 0, 255);
}

void SoftwareRenderDevice::Draw3DLine(vec4 InColor, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	if (!CurrentFrame)
		return;
	float x1, y1, w1, x2, y2, w2;
	if (ProjectLine(ToView(P1), ToView(P2), x1, y1, w1, x2, y2, w2))
		DrawLineScreen(x1, y1, w1, x2, y2, w2, ToColor(InColor), true);
}

void SoftwareRenderDevice::Draw2DLine(vec4 InColor, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	if (!CurrentFrame)
		return;
	float xb = (float)CurrentFrame->XB, yb = (float)CurrentFrame->YB;
	float w1 = P1.z > 0.0f ? 1.0f / P1.z : 1.0f;
	float w2 = P2.z > 0.0f ? 1.0f / P2.z : 1.0f;
	DrawLineScreen(xb + P1.x, yb + P1.y, w1, xb + P2.x, yb + P2.y, w2, ToColor(InColor), false);
}

void SoftwareRenderDevice::Draw2DPoint(vec4 InColor, uint32_t LineFlags, float X1, float Y1, float X2, float Y2, float Z)
{
	if (!CurrentFrame)
		return;
	int x0 = std::max(ClipX0, (int)std::floor((float)CurrentFrame->XB + X1));
	int y0 = std::max(ClipY0, (int)std::floor((float)CurrentFrame->YB + Y1));
	int x1 = std::min(ClipX1, (int)std::ceil((float)CurrentFrame->XB + X2));
	int y1 = std::min(ClipY1, (int)std::ceil((float)CurrentFrame->YB + Y2));
	uint32_t color = ToColor(InColor);
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++)
			Color[(size_t)y * Width + x] = color;
}

void SoftwareRenderDevice::ClearZ()
{
	std::fill(Depth.begin(), Depth.end(), 0.0f);
}

void SoftwareRenderDevice::PushHit(const uint8_t* Data, int Count)
{
}

void SoftwareRenderDevice::PopHit(int Count, bool bForce)
{
}

void SoftwareRenderDevice::ReadPixels(TextureColor* Pixels)
{
	// as the GPU devices return it: blue, green, red, alpha
	memcpy(Pixels, Color.data(), Color.size() * sizeof(uint32_t));
}

void SoftwareRenderDevice::EndFlash()
{
	if (FlashScale == vec4(0.5f, 0.5f, 0.5f, 0.0f) && FlashFog == vec4(0.0f, 0.0f, 0.0f, 0.0f))
		return;

	// the frame scaled, plus the fog colour
	int keep = ClampInt((int)(std::min(FlashScale.x * 2.0f, 1.0f) * 256.0f), 0, 256);
	int fr = ClampInt((int)(FlashFog.x * 255.0f), 0, 255);
	int fg = ClampInt((int)(FlashFog.y * 255.0f), 0, 255);
	int fb = ClampInt((int)(FlashFog.z * 255.0f), 0, 255);
	for (uint32_t& pixel : Color)
	{
		int r = fr + (int)((((pixel >> 16) & 255) * keep) >> 8); if (r > 255) r = 255;
		int g = fg + (int)((((pixel >> 8) & 255) * keep) >> 8); if (g > 255) g = 255;
		int b = fb + (int)(((pixel & 255) * keep) >> 8); if (b > 255) b = 255;
		pixel = 0xff000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
	}
}

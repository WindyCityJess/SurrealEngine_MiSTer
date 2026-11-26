#pragma once

// Software render device: every polygon is rasterized on the CPU into a
// 32-bit frame that the window shows as a bitmap. Written for the MiSTer
// hybrid core build (MISTER_HYBRID), whose ARM has no GPU; it has no
// dependency on the MiSTer and works with any window that can present a bitmap.

#include "RenderDevice/RenderDevice.h"
#include <unordered_map>
#include <vector>
#include <memory>

class SoftwareRenderDevice : public RenderDevice
{
public:
	SoftwareRenderDevice(Widget* viewport);
	~SoftwareRenderDevice();

	void Flush(bool AllowPrecache) override;
	bool Exec(std::string Cmd, OutputDevice& Ar) override;
	void Lock(vec4 FlashScale, vec4 FlashFog, vec4 ScreenClear, uint8_t* HitData, int* HitSize) override;
	void Unlock(bool Blit) override;
	void DrawComplexSurface(SurfaceInfo& Surface, SurfaceFacet& Facet) override;
	void DrawGouraudPolygon(TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags) override;
	void DrawStencilPolygon(StencilPolygonType type, const vec3* Pts, int NumPts) override;
	void DrawTile(TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags) override;
	void Draw3DLine(vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) override;
	void Draw2DLine(vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) override;
	void Draw2DPoint(vec4 Color, uint32_t LineFlags, float X1, float Y1, float X2, float Y2, float Z) override;
	void SetStencilRef(int reference) override;
	void ClearZ() override;
	void PushHit(const uint8_t* Data, int Count) override;
	void PopHit(int Count, bool bForce) override;
	void ReadPixels(TextureColor* Pixels) override;
	void EndFlash() override;
	void SetSceneNode(SceneNode* Frame) override;
	void PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags) override;
	bool SupportsTextureFormat(TextureFormat Format) override;
	void UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL) override;

	// The parts below are public so that a test can drive the rasterizer
	// without a window (Tools/SoftwareRenderTest)
	enum { MaxAttrs = 10, MaxVerts = 64 };

	// A vertex after projection: screen position, 1/z in attr[0], and the
	// other attributes divided by z (perspective correct interpolation)
	struct RVertex
	{
		float x, y;
		float attr[MaxAttrs];
	};

	struct MipLevel
	{
		int width = 0, height = 0;
		bool pow2 = true;
		std::vector<uint32_t> pixels; // 0xAARRGGBB
	};

	struct CachedTexture
	{
		std::vector<MipLevel> mips;
	};

	enum class Blend { Opaque, Translucent, Modulated, Highlighted, SubpixelFont };
	enum class LightSource { Constant, Lightmap, Vertex };

	struct DrawState
	{
		const CachedTexture* tex = nullptr;
		const CachedTexture* lightmap = nullptr;
		const CachedTexture* fogmap = nullptr;
		Blend blend = Blend::Opaque;
		LightSource light = LightSource::Constant;
		bool masked = false;
		bool depthWrite = true;
		bool depthTest = true;
		bool vertexFog = false;
		int constLight[3] = { 256, 256, 256 }; // 256 = 1.0
		uint32_t blendColor = 0xffffffff;
		int numAttrs = 3;
	};

	void SetFrameSize(int width, int height);
	void SetClip(int x0, int y0, int x1, int y1);
	void DrawPolygon(const RVertex* verts, int count, const DrawState& state);
	const uint32_t* GetFrame() const { return Color.data(); }
	int GetFrameWidth() const { return Width; }
	int GetFrameHeight() const { return Height; }
	static void ConvertMip(MipLevel& out, const UnrealMipmap& mip, TextureFormat format, const TextureColor* palette, bool masked);

private:
	const CachedTexture* GetTexture(TextureInfo* info, bool masked);
	void UploadTexture(CachedTexture& tex, TextureInfo& info, bool masked);
	uint32_t ApplyPrecedenceRules(uint32_t PolyFlags) const;
	void SetBlendState(DrawState& state, uint32_t PolyFlags) const;
	vec3 ToView(const vec3& p) const;
	bool ProjectLine(vec3 p1, vec3 p2, float& x1, float& y1, float& w1, float& x2, float& y2, float& w2) const;
	void DrawLineScreen(float x1, float y1, float w1, float x2, float y2, float w2, uint32_t color, bool depthTest);
	int ClipNear(const float (*in)[MaxAttrs + 3], int count, float (*out)[MaxAttrs + 3], int numAttrs) const;
	void DrawViewPolygon(const float (*verts)[MaxAttrs + 3], int count, const DrawState& state, bool fan);

	int Width = 0, Height = 0;
	int ClipX0 = 0, ClipY0 = 0, ClipX1 = 0, ClipY1 = 0;
	std::vector<uint32_t> Color;
	std::vector<float> Depth; // 1/z, larger is closer

	std::unordered_map<uint64_t, std::unique_ptr<CachedTexture>> Textures[2]; // [masked]
	std::unique_ptr<CachedTexture> WhiteTexture;

	SceneNode* CurrentFrame = nullptr;
	mat4 ObjectToView;
	float ScaleX = 1.0f, ScaleY = 1.0f; // view x/z, y/z to pixels
	float CenterX = 0.0f, CenterY = 0.0f;

	vec4 FlashScale = vec4(0.5f, 0.5f, 0.5f, 0.0f);
	vec4 FlashFog = vec4(0.0f);
	bool IsLocked = false;
};

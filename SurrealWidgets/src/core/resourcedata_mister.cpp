#include "surrealwidgets/core/resourcedata.h"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <cmath>

static std::vector<uint8_t> ReadAllBytes(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file)
		throw std::runtime_error("Could not open: " + filename);

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<uint8_t> buffer(size);
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		throw std::runtime_error("Could not read: " + filename);

	return buffer;
}

// MiSTer hybrid core: there is no desktop to ask for a font. The engine's own
// resource loader (SurrealEngine.pk3) provides the fonts the widgets use.
static std::vector<SingleFontData> GetGtkUIFont(const std::string& propertyName)
{
	throw std::runtime_error("No system font on the MiSTer: " + propertyName);
}

std::vector<SingleFontData> ResourceData::LoadSystemFont()
{
	return GetGtkUIFont("font-name");
}

std::vector<SingleFontData> ResourceData::LoadMonospaceSystemFont()
{
	return GetGtkUIFont("monospace-font-name");
}

double ResourceData::GetSystemFontSize()
{
	return 11.0;
}

class ResourceLoaderUnix : public ResourceLoader
{
public:
	std::vector<SingleFontData> LoadFont(const std::string& name) override
	{
		if (name == "system")
			return ResourceData::LoadSystemFont();
		else if (name == "monospace")
			return ResourceData::LoadMonospaceSystemFont();
		else
			return { SingleFontData{ReadAllBytes(name + ".ttf"), ""} };
	}

	std::vector<uint8_t> ReadAllBytes(const std::string& filename) override
	{
		return ::ReadAllBytes(filename);
	}
};

struct ResourceDefaultLoader
{
	ResourceDefaultLoader() { loader = std::make_unique<ResourceLoaderUnix>(); }
	std::unique_ptr<ResourceLoader> loader;
};

static std::unique_ptr<ResourceLoader>& GetLoader()
{
	static ResourceDefaultLoader loader;
	return loader.loader;
}

ResourceLoader* ResourceLoader::Get()
{
	return GetLoader().get();
}

void ResourceLoader::Set(std::unique_ptr<ResourceLoader> instance)
{
	GetLoader() = std::move(instance);
}

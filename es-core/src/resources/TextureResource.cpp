#include "resources/TextureResource.h"

#include "utils/FileSystemUtil.h"
#include "resources/TextureData.h"

TextureDataManager		TextureResource::sTextureDataManager;

std::map< TextureResource::TextureKeyType, std::weak_ptr<TextureResource>> TextureResource::sTextureMap;
std::map< TextureResource::TextureKeyType, std::shared_ptr<TextureResource>> TextureResource::sPermanentTextureMap; // FCAWEAK

std::set<TextureResource*> 	TextureResource::sAllTextures;

TextureResource::TextureResource(const std::string& path, bool tile, bool dynamic, Vector2f maxSize) : mTextureData(nullptr), mForceLoad(false)
{
	// Create a texture data object for this texture
	if (!path.empty())
	{
		// If there is a path then the 'dynamic' flag tells us whether to use the texture
		// data manager to manage loading/unloading of this texture
		std::shared_ptr<TextureData> data;
		if (dynamic)
		{			
			data = sTextureDataManager.add(this, tile);
			data->setMaxSize(maxSize);
			data->initFromPath(path);

			// Force the texture manager to load it using a blocking load
			sTextureDataManager.load(data, true); // 
		}
		else
		{
			mTextureData = std::shared_ptr<TextureData>(new TextureData(tile));
			
			data = mTextureData;
			data->setMaxSize(maxSize);
			data->initFromPath(path);
			// Load it so we can read the width/height
			data->load();
		}

		mSize = Vector2i((int)data->width(), (int)data->height());
		mSourceSize = Vector2f(data->sourceWidth(), data->sourceHeight());
	}
	else
	{
		// Create a texture managed by this class because it cannot be dynamically loaded and unloaded
		mTextureData = std::shared_ptr<TextureData>(new TextureData(tile));
	}
	sAllTextures.insert(this);
}

TextureResource::~TextureResource()
{
	if (mTextureData == nullptr)
		sTextureDataManager.remove(this);

	if (sAllTextures.size() > 0)
	{
		auto pthis = sAllTextures.find(this);
		if (pthis != sAllTextures.end())
			sAllTextures.erase(pthis);
	}
}

void TextureResource::initFromPixels(const unsigned char* dataRGBA, size_t width, size_t height)
{
	// This is only valid if we have a local texture data object
	assert(mTextureData != nullptr);
	mTextureData->releaseVRAM();
	mTextureData->releaseRAM();
	mTextureData->initFromRGBA(dataRGBA, width, height);
	// Cache the image dimensions
	mSize = Vector2i((int)width, (int)height);
	mSourceSize = Vector2f(mTextureData->sourceWidth(), mTextureData->sourceHeight());
}

void TextureResource::initFromMemory(const char* data, size_t length)
{
	// This is only valid if we have a local texture data object
	assert(mTextureData != nullptr);
	mTextureData->releaseVRAM();
	mTextureData->releaseRAM();
	mTextureData->initImageFromMemory((const unsigned char*)data, length);
	// Get the size from the texture data
	mSize = Vector2i((int)mTextureData->width(), (int)mTextureData->height());
	mSourceSize = Vector2f(mTextureData->sourceWidth(), mTextureData->sourceHeight());
}

const Vector2i TextureResource::getSize() const
{
	return mSize;
}

bool TextureResource::isTiled() const
{
	if (mTextureData != nullptr)
		return mTextureData->tiled();
	std::shared_ptr<TextureData> data = sTextureDataManager.get(this);
	return data->tiled();
}

bool TextureResource::bind()
{
	if (mTextureData != nullptr)
	{
		mTextureData->uploadAndBind();
		return true;
	}
	else
	{
		return sTextureDataManager.bind(this);
	}
}

void TextureResource::resetCache()
{
	sPermanentTextureMap.clear();
	sTextureMap.clear();
}

std::shared_ptr<TextureResource> TextureResource::get(const std::string& path, bool tile, bool forceLoad, bool dynamic, Vector2f maxSize)
{
	std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();

	const std::string canonicalPath = Utils::FileSystem::getCanonicalPath(path);
	if(canonicalPath.empty())
	{
		std::shared_ptr<TextureResource> tex(new TextureResource("", tile, false, maxSize));
		rm->addReloadable(tex); //make sure we get properly deinitialized even though we do nothing on reinitialization
		return tex;
	}

	TextureKeyType key(canonicalPath, tile);
	auto foundTexture = sTextureMap.find(key);
	if(foundTexture != sTextureMap.cend())
	{
		if (!foundTexture->second.expired()) // FCAWEAK
		{
			std::shared_ptr<TextureResource> rc = foundTexture->second.lock();

			if (maxSize != Vector2f(0, 0) && TextureData::OPTIMIZEVRAM)
			{
				auto dt = sTextureDataManager.get(rc.get());
				if (dt != nullptr)
					dt->setMaxSize(maxSize);
			}

			return rc;
		}
		else
			sTextureMap.erase(foundTexture);
	}

	auto permanentTexture = sPermanentTextureMap.find(key);
	if (permanentTexture != sPermanentTextureMap.cend())
	{
		std::shared_ptr<TextureResource> rc = permanentTexture->second;

		if (maxSize != Vector2f(0, 0) && TextureData::OPTIMIZEVRAM)
		{
			auto dt = sTextureDataManager.get(rc.get());
			if (dt != nullptr)
				dt->setMaxSize(maxSize);
		}

		return rc;
	}
	
	// need to create it
	std::shared_ptr<TextureResource> tex;
	tex = std::shared_ptr<TextureResource>(new TextureResource(key.first, tile, dynamic, maxSize));
	std::shared_ptr<TextureData> data = sTextureDataManager.get(tex.get());

	// is it an SVG?
	// if(key.first.substr(key.first.size() - 4, std::string::npos) != ".svg") // FCATMP
	{
		// Probably not. Add it to our map. We don't add SVGs because 2 svgs might be rasterized at different sizes
		// FCA useless -> If the svg is too small, it will be reloaded bigger with setSourceSize...

		if (canonicalPath.length() > 0 && canonicalPath[0] == ':')
			sPermanentTextureMap[key] = std::shared_ptr<TextureResource>(tex); // FCAWEAK
		else
			sTextureMap[key] = std::shared_ptr<TextureResource>(tex); // FCAWEAK
	}

	
	// Add it to the reloadable list, exclusion for splash.svg manually managed
#ifdef WIN32
	if (path != ":/splash.svg")
#endif
		rm->addReloadable(tex);

	if (data != nullptr)
		data->setMaxSize(maxSize);

	// Force load it if necessary. Note that it may get dumped from VRAM if we run low
	if (forceLoad)
	{
		tex->mForceLoad = forceLoad;
		data->load();
	}

	return tex;
}

// For scalable source images in textures we want to set the resolution to rasterize at
void TextureResource::rasterizeAt(size_t width, size_t height)
{
	std::shared_ptr<TextureData> data;
	if (mTextureData != nullptr)
		data = mTextureData;
	else
		data = sTextureDataManager.get(this);
	mSourceSize = Vector2f((float)width, (float)height);
	data->setSourceSize((float)width, (float)height);

	if (mForceLoad || (mTextureData != nullptr))
		if (!data->isLoaded()) // FCATMP
			data->load();
}

Vector2f TextureResource::getSourceImageSize() const
{
	return mSourceSize;
}

bool TextureResource::isInitialized() const
{
	return true;
}

size_t TextureResource::getTotalMemUsage()
{
	size_t total = 0;
	// Count up all textures that manage their own texture data
	for (auto tex : sAllTextures)
	{
		if (tex->mTextureData != nullptr)
			total += tex->mTextureData->getVRAMUsage();
	}
	// Now get the committed memory from the manager
	total += sTextureDataManager.getCommittedSize();
	// And the size of the loading queue
	total += sTextureDataManager.getQueueSize();
	return total;
}

size_t TextureResource::getTotalTextureSize()
{
	size_t total = 0;
	// Count up all textures that manage their own texture data
	for (auto tex : sAllTextures)
	{
		if (tex->mTextureData != nullptr)
			total += tex->getSize().x() * tex->getSize().y() * 4;
	}
	// Now get the total memory from the manager
	total += sTextureDataManager.getTotalSize();
	return total;
}

void TextureResource::unload()
{
	// Release the texture's resources
	std::shared_ptr<TextureData> data;
	if (mTextureData == nullptr)
		data = sTextureDataManager.get(this);
	else
		data = mTextureData;

	data->releaseVRAM();
	data->releaseRAM();
}

void TextureResource::reload()
{
	// For dynamically loaded textures the texture manager will load them on demand.
	// For manually loaded textures we have to reload them here
	if (mTextureData && !mTextureData->isLoaded())
		mTextureData->load();
	else if (mTextureData == nullptr)
		sTextureDataManager.get(this);
}

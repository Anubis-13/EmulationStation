#include "resources/TextureData.h"

#include "math/Misc.h"
#include "resources/ResourceManager.h"
#include "ImageIO.h"
#include "Log.h"
#include "platform.h"
#include GLHEADER
#include <nanosvg/nanosvg.h>
#include <nanosvg/nanosvgrast.h>
#include <assert.h>
#include <string.h>

#define DPI 96

bool TextureData::OPTIMIZEVRAM = false;

TextureData::TextureData(bool tile) : mTile(tile), mTextureID(0), mDataRGBA(nullptr), mScalable(false),
									  mWidth(0), mHeight(0), mSourceWidth(0.0f), mSourceHeight(0.0f), mMaxSize(Vector2f(0,0)), mPackedSize(Vector2i(0,0)), mBaseSize(Vector2i(0, 0))
{
	
}

TextureData::~TextureData()
{
	releaseVRAM();
	releaseRAM();
}

void TextureData::initFromPath(const std::string& path)
{
	// Just set the path. It will be loaded later
	mPath = path;
	// Only textures with paths are reloadable
	mReloadable = true;
}

bool TextureData::initSVGFromMemory(const unsigned char* fileData, size_t length)
{
	// If already initialised then don't read again
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA)
		return true;

	// nsvgParse excepts a modifiable, null-terminated string
	char* copy = (char*)malloc(length + 1);
	assert(copy != NULL);
	memcpy(copy, fileData, length);
	copy[length] = '\0';

	NSVGimage* svgImage = nsvgParse(copy, "px", DPI);
	free(copy);
	if (!svgImage)
	{
		LOG(LogError) << "Error parsing SVG image.";
		return false;
	}

	// We want to rasterise this texture at a specific resolution. If the source size
	// variables are set then use them otherwise set them from the parsed file
	if ((mSourceWidth == 0.0f) && (mSourceHeight == 0.0f))
	{
		mSourceWidth = svgImage->width;
		mSourceHeight = svgImage->height;
	}
	else 
		mSourceWidth = (mSourceHeight * svgImage->width) / svgImage->height; // FCATMP : Always keep source aspect ratio

//	mWidth = (size_t)Math::round(mSourceWidth);
//	mHeight = (size_t)Math::round(mSourceHeight);

	mWidth = (int) mSourceWidth;
	mHeight = (int) mSourceHeight;

	if (mWidth == 0)
	{
		// auto scale width to keep aspect
		mWidth = (size_t)Math::round(((float)mHeight / svgImage->height) * svgImage->width);
	}
	else if (mHeight == 0)
	{
		// auto scale height to keep aspect
		mHeight = (size_t)Math::round(((float)mWidth / svgImage->width) * svgImage->height);
	}

	mBaseSize = Vector2i(mWidth, mHeight);
	
	if (mMaxSize.x() > 0 && mMaxSize.y() > 0 && mHeight < mMaxSize.y() && mWidth < mMaxSize.y()) // FCATMP
	{
		Vector2i sz = ImageIO::adjustPictureSize(Vector2i(mWidth, mHeight), Vector2i(mMaxSize.x(), mMaxSize.y()));
		mWidth = sz.x();		
		mHeight = sz.y();
		mWidth = (int)((mHeight * svgImage->width) / svgImage->height);
	}
	
	if (OPTIMIZEVRAM && mMaxSize.x() > 0 && mMaxSize.y() > 0 && (mWidth > mMaxSize.x() || mHeight > mMaxSize.y()))
	{
		Vector2i sz = ImageIO::adjustPictureSize(Vector2i(mWidth, mHeight), Vector2i(mMaxSize.x(), mMaxSize.y()));
		mWidth = sz.x();
		mHeight = sz.y();	
		mWidth = (mHeight * svgImage->width) / svgImage->height;

		mPackedSize = Vector2i(mWidth, mHeight);
	}
	else
		mPackedSize = Vector2i(0, 0);
	
	unsigned char* dataRGBA = new unsigned char[mWidth * mHeight * 4];

	NSVGrasterizer* rast = nsvgCreateRasterizer();
	nsvgRasterize(rast, svgImage, 0, 0, mHeight / svgImage->height, dataRGBA, (int)mWidth, (int)mHeight, (int)mWidth * 4);
	nsvgDeleteRasterizer(rast);

	ImageIO::flipPixelsVert(dataRGBA, mWidth, mHeight);

	mDataRGBA = dataRGBA;

	return true;
}

bool TextureData::isRequiredTextureSizeOk()
{
	if (!OPTIMIZEVRAM)
		return true;

	if (mPackedSize == Vector2i(0, 0))
		return true;

	if (mBaseSize == Vector2i(0, 0))
		return true;

	if (mMaxSize == Vector2f(0, 0))
		return true;

	if ((int) mMaxSize.x() <= mPackedSize.x() || (int) mMaxSize.y() <= mPackedSize.y())
		return true;

	if (mBaseSize.x() <= mPackedSize.x() || mBaseSize.y() <= mPackedSize.y())
		return true;

	return false;
}

bool TextureData::initImageFromMemory(const unsigned char* fileData, size_t length)
{
	size_t width, height;

	// If already initialised then don't read again
	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mDataRGBA)
			return true;
	}
	
	unsigned char* imageRGBA = ImageIO::loadFromMemoryRGBA32Ex((const unsigned char*)(fileData), length, width, height, OPTIMIZEVRAM ? mMaxSize.x() : 0, mMaxSize.y(), mBaseSize, mPackedSize);
	if (imageRGBA == NULL)
	{
		LOG(LogError) << "Could not initialize texture from memory, invalid data!  (file path: " << mPath << ", data ptr: " << (size_t)fileData << ", reported size: " << length << ")";
		return false;
	}

	mSourceWidth = (float) width;
	mSourceHeight = (float) height;
	mScalable = false;

	return initFromRGBAEx(imageRGBA, width, height);
}

bool TextureData::initFromRGBA(const unsigned char* dataRGBA, size_t width, size_t height)
{
	// If already initialised then don't read again
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA)
		return true;

	// Take a copy
	mDataRGBA = new unsigned char[width * height * 4];
	memcpy(mDataRGBA, dataRGBA, width * height * 4);
	mWidth = width;
	mHeight = height;
	return true;
}

bool TextureData::initFromRGBAEx(unsigned char* dataRGBA, size_t width, size_t height)
{
	// If already initialised then don't read again
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA)
		return true;

	// Take a copy
	mDataRGBA = dataRGBA;
	mWidth = width;
	mHeight = height;

	return true;
}

bool TextureData::load()
{
	bool retval = false;

	// Need to load. See if there is a file
	if (!mPath.empty())
	{
#ifdef WIN32	
		//TRACE("TextureData::load(" << mPath << ", " << mTextureID << ")")
#endif

		std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();

		const ResourceData& data = rm->getFileData(mPath);
		// is it an SVG?
		if (mPath.substr(mPath.size() - 4, std::string::npos) == ".svg")
		{
			mScalable = true; // ??? interest ?
			retval = initSVGFromMemory((const unsigned char*)data.ptr.get(), data.length);
		}
		else
			retval = initImageFromMemory((const unsigned char*)data.ptr.get(), data.length);
	}

	

	return retval;
}

bool TextureData::isLoaded()
{
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA || (mTextureID != 0))
		return true;

	return false;
}

bool TextureData::uploadAndBind()
{
	// See if it's already been uploaded
	std::unique_lock<std::mutex> lock(mMutex);
	if (mTextureID != 0)
	{
		glBindTexture(GL_TEXTURE_2D, mTextureID);
	}
	else
	{
		// Load it if necessary
		if (!mDataRGBA)
		{
			return false;
		}
		// Make sure we're ready to upload
		if ((mWidth == 0) || (mHeight == 0) || (mDataRGBA == nullptr))
			return false;

		glGetError();
		//now for the openGL texture stuff
		glGenTextures(1, &mTextureID);
		glBindTexture(GL_TEXTURE_2D, mTextureID);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)mWidth, (GLsizei)mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, mDataRGBA);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		const GLint wrapMode = mTile ? GL_REPEAT : GL_CLAMP_TO_EDGE;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
	}

	return true;
}

void TextureData::releaseVRAM()
{
	std::unique_lock<std::mutex> lock(mMutex);
	if (mTextureID != 0)
	{
		glDeleteTextures(1, &mTextureID);
		mTextureID = 0;
	}
}

void TextureData::releaseRAM()
{
	std::unique_lock<std::mutex> lock(mMutex);
	delete[] mDataRGBA;
	mDataRGBA = 0;
}

size_t TextureData::width()
{
	if (mWidth == 0)
		load();
	return mWidth;
}

size_t TextureData::height()
{
	if (mHeight == 0)
		load();
	return mHeight;
}

float TextureData::sourceWidth()
{
	if (mSourceWidth == 0)
		load();
	return mSourceWidth;
}

float TextureData::sourceHeight()
{
	if (mSourceHeight == 0)
		load();
	return mSourceHeight;
}

void TextureData::setSourceSize(float width, float height)
{
	if (mScalable)
	{
		//if ((mSourceWidth != width) || (mSourceHeight != height))
		if (mSourceHeight < height) // FCATMP
		{
			mSourceWidth = width;
			mSourceHeight = height;
			releaseVRAM();
			releaseRAM();
		}
	}
}

size_t TextureData::getVRAMUsage()
{
	if ((mTextureID != 0) || (mDataRGBA != nullptr))
		return mWidth * mHeight * 4;
	else
		return 0;
}

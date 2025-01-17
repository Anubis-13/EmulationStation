#include "GuiComponent.h"

#include "animations/Animation.h"
#include "animations/AnimationController.h"
#include "Log.h"
#include "Renderer.h"
#include "ThemeData.h"
#include "Window.h"
#include <algorithm>


#if defined(_WIN32)
#define _conv(x) GuiTextTool::convertFromWideString(L ## x)
#else
#define _conv(x) x
#endif

#include <fstream>
#include <string>
#include "resources\ResourceManager.h"

std::vector<LocalizationItem*> GuiTextTool::mItems;
std::string GuiTextTool::mCurrentLanguage = "en";
bool GuiTextTool::mCurrentLanguageLoaded = false;

void GuiTextTool::setLanguage(std::string lang)
{
	mCurrentLanguage = lang;
	mCurrentLanguageLoaded = false;
}

void GuiTextTool::ensureLocalisation()
{
	if (mCurrentLanguageLoaded)
	{
		if (Settings::getInstance()->getString("Language") == mCurrentLanguage)
			return;

		mCurrentLanguage = Settings::getInstance()->getString("Language");
	}

	mCurrentLanguageLoaded = true;

	for (std::vector<LocalizationItem*>::const_iterator it = mItems.cbegin(); it != mItems.cend(); ++it)
		delete (*it);

	mItems.clear();

	std::string xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/"+ mCurrentLanguage +"/emulationstation2.po");
	if (Utils::FileSystem::exists(xmlpath))
	{				
		LocalizationItem* currentItem = NULL;

		std::ifstream file(xmlpath);
		std::string str;
		while (std::getline(file, str))
		{
			if (str.length() > 0 && str[0] == '#')
			{
				if (currentItem != NULL && currentItem->msgid.length() > 0 && currentItem->msgstr.length() > 0)
					mItems.push_back(currentItem);

				currentItem = new LocalizationItem();
			}

			if (currentItem != NULL && str.find("msgid") == 0)
			{
				auto start = str.find("\"");
				if (start != std::string::npos)
				{
					auto end = str.find("\"", start + 1);
					if (end != std::string::npos)
						currentItem->msgid = str.substr(start + 1, end - start - 1);
				}
			}

			if (currentItem != NULL && str.find("msgstr") == 0)
			{
				auto start = str.find("\"");
				if (start != std::string::npos)
				{
					auto end = str.find("\"", start + 1);
					if (end != std::string::npos)
						currentItem->msgstr = str.substr(start + 1, end - start - 1);
				}
			}
		}

		if (currentItem != NULL)
			delete currentItem;
	}
}


const std::string GuiTextTool::localize(const std::string text)
{
	ensureLocalisation();

	for (std::vector<LocalizationItem*>::const_iterator it = mItems.cbegin(); it != mItems.cend(); ++it)
	{
		if (text == (*it)->msgid)
			return (*it)->msgstr;		
	}

	return text;	
}

bool GuiComponent::ALLOWANIMATIONS = true;

GuiComponent::GuiComponent(Window* window) : mWindow(window), mParent(NULL), mOpacity(255),
	mPosition(Vector3f::Zero()), mOrigin(Vector2f::Zero()), mRotationOrigin(0.5, 0.5),
	mSize(Vector2f::Zero()), mTransform(Transform4x4f::Identity()), mIsProcessing(false)
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		mAnimationMap[i] = NULL;
}

GuiComponent::~GuiComponent()
{
	mWindow->removeGui(this);

	cancelAllAnimations();

	if(mParent)
		mParent->removeChild(this);

	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->setParent(NULL);
}

bool GuiComponent::input(InputConfig* config, Input input)
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		if(getChild(i)->input(config, input))
			return true;
	}

	return false;
}

void GuiComponent::updateSelf(int deltaTime)
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		advanceAnimation(i, deltaTime);
}

void GuiComponent::updateChildren(int deltaTime)
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		getChild(i)->update(deltaTime);
	}
}

void GuiComponent::update(int deltaTime)
{
	updateSelf(deltaTime);
	updateChildren(deltaTime);
}

void GuiComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	renderChildren(trans);
}

void GuiComponent::renderChildren(const Transform4x4f& transform) const
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		getChild(i)->render(transform);
	}
}

Vector3f GuiComponent::getPosition() const
{
	return mPosition;
}

void GuiComponent::setPosition(float x, float y, float z)
{
	mPosition = Vector3f(x, y, z);
	onPositionChanged();
}

Vector2f GuiComponent::getOrigin() const
{
	return mOrigin;
}

void GuiComponent::setOrigin(float x, float y)
{
	mOrigin = Vector2f(x, y);
	onOriginChanged();
}

Vector2f GuiComponent::getRotationOrigin() const
{
	return mRotationOrigin;
}

void GuiComponent::setRotationOrigin(float x, float y)
{
	mRotationOrigin = Vector2f(x, y);
}

Vector2f GuiComponent::getSize() const
{
	return mSize;
}

void GuiComponent::setSize(float w, float h)
{
	mSize = Vector2f(w, h);
	onSizeChanged();
}

float GuiComponent::getRotation() const
{
	return mRotation;
}

void GuiComponent::setRotation(float rotation)
{
	mRotation = rotation;
}

float GuiComponent::getScale() const
{
	return mScale;
}

void GuiComponent::setScale(float scale)
{
	mScale = scale;
}

float GuiComponent::getZIndex() const
{
	return mZIndex;
}

void GuiComponent::setZIndex(float z)
{
	mZIndex = z;
}

float GuiComponent::getDefaultZIndex() const
{	
	return mDefaultZIndex;
}

void GuiComponent::setDefaultZIndex(float z)
{
	mDefaultZIndex = z;
}

Vector2f GuiComponent::getCenter() const
{
	return Vector2f(mPosition.x() - (getSize().x() * mOrigin.x()) + getSize().x() / 2,
	                mPosition.y() - (getSize().y() * mOrigin.y()) + getSize().y() / 2);
}

bool GuiComponent::isChild(GuiComponent* cmp)
{
	for (auto i = mChildren.cbegin(); i != mChildren.cend(); i++)
		if (*i == cmp)
			return true;
	
	return false;
}

//Children stuff.
void GuiComponent::addChild(GuiComponent* cmp)
{
	mChildren.push_back(cmp);

	if(cmp->getParent())
		cmp->getParent()->removeChild(cmp);

	cmp->setParent(this);
}

void GuiComponent::removeChild(GuiComponent* cmp)
{
	if(!cmp->getParent())
		return;

	if(cmp->getParent() != this)
	{
		LOG(LogError) << "Tried to remove child from incorrect parent!";
	}

	cmp->setParent(NULL);

	for(auto i = mChildren.cbegin(); i != mChildren.cend(); i++)
	{
		if(*i == cmp)
		{
			mChildren.erase(i);
			return;
		}
	}
}

void GuiComponent::clearChildren()
{
	mChildren.clear();
}

void GuiComponent::sortChildren()
{
	std::stable_sort(mChildren.begin(), mChildren.end(),  [](GuiComponent* a, GuiComponent* b) {
		return b->getZIndex() > a->getZIndex();
	});
}

unsigned int GuiComponent::getChildCount() const
{
	return (int)mChildren.size();
}

GuiComponent* GuiComponent::getChild(unsigned int i) const
{
	return mChildren.at(i);
}

void GuiComponent::setParent(GuiComponent* parent)
{
	mParent = parent;
}

GuiComponent* GuiComponent::getParent() const
{
	return mParent;
}

unsigned char GuiComponent::getOpacity() const
{
	return mOpacity;
}

void GuiComponent::setOpacity(unsigned char opacity)
{
	mOpacity = opacity;
	for(auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
	{
		(*it)->setOpacity(opacity);
	}
}

const Transform4x4f& GuiComponent::getTransform()
{
	mTransform = Transform4x4f::Identity();
	mTransform.translate(mPosition);
	if (mScale != 1.0)
	{
		mTransform.scale(mScale);
	}
	if (mRotation != 0.0)
	{
		// Calculate offset as difference between origin and rotation origin
		Vector2f rotationSize = getRotationSize();
		float xOff = (mOrigin.x() - mRotationOrigin.x()) * rotationSize.x();
		float yOff = (mOrigin.y() - mRotationOrigin.y()) * rotationSize.y();

		// transform to offset point
		if (xOff != 0.0 || yOff != 0.0)
			mTransform.translate(Vector3f(xOff * -1, yOff * -1, 0.0f));

		// apply rotation transform
		mTransform.rotateZ(mRotation);

		// Tranform back to original point
		if (xOff != 0.0 || yOff != 0.0)
			mTransform.translate(Vector3f(xOff, yOff, 0.0f));
	}
	mTransform.translate(Vector3f(mOrigin.x() * mSize.x() * -1, mOrigin.y() * mSize.y() * -1, 0.0f));
	return mTransform;
}

void GuiComponent::setValue(const std::string& /*value*/)
{
}

std::string GuiComponent::getValue() const
{
	return "";
}

void GuiComponent::setTag(const std::string& value)
{
	mTag = value;
}

std::string GuiComponent::getTag() const
{
	return mTag;
}

void GuiComponent::textInput(const char* text)
{
	for(auto iter = mChildren.cbegin(); iter != mChildren.cend(); iter++)
	{
		(*iter)->textInput(text);
	}
}

void GuiComponent::setAnimation(Animation* anim, int delay, std::function<void()> finishedCallback, bool reverse, unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);

	AnimationController* oldAnim = mAnimationMap[slot];
	mAnimationMap[slot] = new AnimationController(anim, delay, finishedCallback, reverse);

	if(oldAnim)
		delete oldAnim;
}

bool GuiComponent::stopAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	if(mAnimationMap[slot])
	{
		delete mAnimationMap[slot];
		mAnimationMap[slot] = NULL;
		return true;
	}else{
		return false;
	}
}

bool GuiComponent::cancelAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	if(mAnimationMap[slot])
	{
		mAnimationMap[slot]->removeFinishedCallback();
		delete mAnimationMap[slot];
		mAnimationMap[slot] = NULL;
		return true;
	}else{
		return false;
	}
}

bool GuiComponent::finishAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	if(mAnimationMap[slot])
	{
		// skip to animation's end
		const bool done = mAnimationMap[slot]->update(mAnimationMap[slot]->getAnimation()->getDuration() - mAnimationMap[slot]->getTime());
		assert(done);

		delete mAnimationMap[slot]; // will also call finishedCallback
		mAnimationMap[slot] = NULL;
		return true;
	}else{
		return false;
	}
}

bool GuiComponent::advanceAnimation(unsigned char slot, unsigned int time)
{
	assert(slot < MAX_ANIMATIONS);
	AnimationController* anim = mAnimationMap[slot];
	if(anim)
	{
		bool done = anim->update(time);
		if(done)
		{
			mAnimationMap[slot] = NULL;
			delete anim;
		}
		return true;
	}else{
		return false;
	}
}

void GuiComponent::stopAllAnimations()
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		stopAnimation(i);
}

void GuiComponent::cancelAllAnimations()
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		cancelAnimation(i);
}

bool GuiComponent::isAnimationPlaying(unsigned char slot) const
{
	return mAnimationMap[slot] != NULL;
}

bool GuiComponent::isAnimationReversed(unsigned char slot) const
{
	assert(mAnimationMap[slot] != NULL);
	return mAnimationMap[slot]->isReversed();
}

int GuiComponent::getAnimationTime(unsigned char slot) const
{
	assert(mAnimationMap[slot] != NULL);
	return mAnimationMap[slot]->getTime();
}

void GuiComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "");
	if(!elem)
		return;

	using namespace ThemeFlags;
	if(properties & POSITION && elem->has("pos"))
	{
		Vector2f denormalized = elem->get<Vector2f>("pos") * scale;
		setPosition(Vector3f(denormalized.x(), denormalized.y(), 0));
	}

	if(properties & ThemeFlags::SIZE && elem->has("size"))
		setSize(elem->get<Vector2f>("size") * scale);

	// position + size also implies origin
	if((properties & ORIGIN || (properties & POSITION && properties & ThemeFlags::SIZE)) && elem->has("origin"))
		setOrigin(elem->get<Vector2f>("origin"));

	if(properties & ThemeFlags::ROTATION) {
		if(elem->has("rotation"))
			setRotationDegrees(elem->get<float>("rotation"));
		if(elem->has("rotationOrigin"))
			setRotationOrigin(elem->get<Vector2f>("rotationOrigin"));
	}

	if(properties & ThemeFlags::Z_INDEX && elem->has("zIndex"))
		setZIndex(elem->get<float>("zIndex"));
	else
		setZIndex(getDefaultZIndex());
}

void GuiComponent::updateHelpPrompts()
{
	if(getParent())
	{
		getParent()->updateHelpPrompts();
		return;
	}

	std::vector<HelpPrompt> prompts = getHelpPrompts();

	if(mWindow->peekGui() == this)
		mWindow->setHelpPrompts(prompts, getHelpStyle());
}

HelpStyle GuiComponent::getHelpStyle()
{
	return HelpStyle();
}

bool GuiComponent::isProcessing() const
{
	return mIsProcessing;
}

void GuiComponent::onShow()
{
	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->onShow();
}

void GuiComponent::onHide()
{
	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->onHide();
}

void GuiComponent::onScreenSaverActivate()
{
	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->onScreenSaverActivate();
}

void GuiComponent::onScreenSaverDeactivate()
{
	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->onScreenSaverDeactivate();
}

void GuiComponent::topWindow(bool isTop)
{
	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->topWindow(isTop);
}
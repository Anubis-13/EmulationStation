#pragma once
#ifndef ES_CORE_COMPONENTS_MENU_COMPONENT_H
#define ES_CORE_COMPONENTS_MENU_COMPONENT_H

#include "components/ComponentGrid.h"
#include "components/ComponentList.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "utils/StringUtil.h"
#include "Renderer.h"

class ButtonComponent;
class ImageComponent;

std::shared_ptr<ComponentGrid> makeButtonGrid(Window* window, const std::vector< std::shared_ptr<ButtonComponent> >& buttons);
std::shared_ptr<ComponentGrid> makeMultiDimButtonGrid(Window* window, const std::vector< std::vector< std::shared_ptr<ButtonComponent> > >& buttons, float outerWidth);
std::shared_ptr<ImageComponent> makeArrow(Window* window);

#define TITLE_VERT_PADDING (Renderer::getScreenHeight()*0.0637f)

class MenuComponent : public GuiComponent
{
public:
	MenuComponent(Window* window, std::string title, const std::shared_ptr<Font>& titleFont = Font::get(FONT_SIZE_LARGE));

	void onSizeChanged() override;

	inline void addRow(const ComponentListRow& row, bool setCursorHere = false) { mList->addRow(row, setCursorHere); updateSize(); }

	void addWithLabel(const std::string& label, const std::shared_ptr<GuiComponent>& comp, bool setCursorHere = false, bool invert_when_selected = true);

	void addButton(const std::string& label, const std::string& helpText, const std::function<void()>& callback);

	void setTitle(const char* title, const std::shared_ptr<Font>& font);

	inline void setCursorToList() { mGrid.setCursorTo(mList); }
	inline void setCursorToButtons() { assert(mButtonGrid); mGrid.setCursorTo(mButtonGrid); }

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void updateSize();
	void updateGrid();
	float getButtonGridHeight() const;

	NinePatchComponent mBackground;
	ComponentGrid mGrid;

	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<ComponentList> mList;
	std::shared_ptr<ComponentGrid> mButtonGrid;
	std::vector< std::shared_ptr<ButtonComponent> > mButtons;
};

#endif // ES_CORE_COMPONENTS_MENU_COMPONENT_H

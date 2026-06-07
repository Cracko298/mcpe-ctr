#include "OptionsItem.h"
#include "../../Minecraft.h"
#include "../../../util/Mth.h"
OptionsItem::OptionsItem( std::string label, GuiElement* element )
: GuiElementContainer(false, true, 0, 0, 24, 12),
  label(label) {
	  addChild(element);
}

void OptionsItem::setupPositions() {
	int currentHeight = 0;
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->x = x + width - (*it)->width - 15;
		(*it)->y = y + currentHeight;
		currentHeight += (*it)->height;
	}
	height = currentHeight;
}

void OptionsItem::render( Minecraft* minecraft, int xm, int ym ) {
	int yOffset = (height - 8) / 2;
	
	int elementX = x + width;
	if (!children.empty()) {
		elementX = children[0]->x;
	}
	int availableWidth = elementX - x - 5;
	if (availableWidth < 10) availableWidth = 10;
	
	std::string clippedLabel = minecraft->font->clipToWidth(label, availableWidth);
	
	minecraft->font->draw(clippedLabel, (float)x, (float)y + yOffset, 0x909090, false);
	super::render(minecraft, xm, ym);
}
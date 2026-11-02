#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "DecorPopup.hpp"

using namespace geode::prelude;

class $modify(DecorEditorUI, EditorUI) {
	bool init(LevelEditorLayer* lel) {
		if (!EditorUI::init(lel)) return false;

		auto win = CCDirector::get()->getWinSize();

		auto menu = CCMenu::create();
		menu->setID("decorador-ia-menu"_spr);
		menu->setPosition({0, 0});

		auto label = CCLabelBMFont::create("IA", "bigFont.fnt");
		auto spr = CircleButtonSprite::create(label, CircleBaseColor::Green, CircleBaseSize::Medium);
		spr->setScale(0.6f);

		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(DecorEditorUI::onOpenDecor));
		// Barra superior, entre la papelera y el deslizador. Ajusta si choca con otro boton
		btn->setPosition({230.f, win.height - 22.f});
		menu->addChild(btn);

		this->addChild(menu, 100);
		return true;
	}

	void onOpenDecor(CCObject*) {
		if (auto popup = DecorPopup::create()) popup->show();
	}
};

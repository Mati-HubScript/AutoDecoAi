#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <set>

using namespace geode::prelude;

class DecorPopup : public Popup<> {
protected:
	TextInput* m_input = nullptr;
	CCLabelBMFont* m_info = nullptr;
	CCLabelBMFont* m_status = nullptr;
	CCMenuItemSpriteExtra* m_sendBtn = nullptr;
	bool m_busy = false;
	CCRect m_bounds;
	std::set<int> m_allowed;
	std::string m_provider;
	EventListener<web::WebTask> m_listener;

	bool setup() override;
	void onSend(CCObject*);
	void onCloseBtn(CCObject*);
	void setStatus(std::string const& text, ccColor3B color);
	void handleResponse(web::WebResponse* res);
	void applyDecoration(std::string const& text);

public:
	static DecorPopup* create();
};

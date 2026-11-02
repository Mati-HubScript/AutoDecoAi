#include "DecorPopup.hpp"
#include <algorithm>
#include <limits>
#include <cctype>

// matjson: asArray() devuelve Result<vector&>; copiamos el vector o devolvemos uno vacio
#include <fstream>
#include <filesystem>

// Lee la API key: primero del archivo de texto en la carpeta de guardado del mod
// (no sale del telefono ni se sube a GitHub); si esta vacio, usa el ajuste del mod.
// Si no hay ninguna, crea el archivo vacio y avisa con la ruta exacta.
static std::string loadKey(std::string const& settingId, std::string const& fileName) {
	auto path = Mod::get()->getSaveDir() / fileName;

	std::string key;
	{
		std::ifstream f(path);
		std::string line;
		if (f && std::getline(f, line)) key = line;
	}
	// limpiar espacios, saltos de linea y marca BOM
	std::string clean;
	for (unsigned char c : key) {
		if (!std::isspace(c) && c != 0xEF && c != 0xBB && c != 0xBF) clean += static_cast<char>(c);
	}
	if (!clean.empty()) return clean;

	auto fromSetting = Mod::get()->getSettingValue<std::string>(settingId);
	if (!fromSetting.empty()) return fromSetting;

	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (!std::filesystem::exists(path)) std::ofstream(path).close();

	FLAlertLayer::create(
		"Falta la API key",
		fmt::format("Pega tu key (una sola linea) en este archivo, guardalo y pulsa Generar otra vez:\n{}", path.string()),
		"OK"
	)->show();
	return "";
}

static std::vector<matjson::Value> asVec(matjson::Value const& v) {
	if (auto a = v.asArray(); a.isOk()) return a.unwrap();
	return {};
}

static constexpr ccColor3B COL_OK{120, 230, 150};
static constexpr ccColor3B COL_ERR{255, 110, 110};
static constexpr ccColor3B COL_DIM{170, 170, 185};

DecorPopup* DecorPopup::create() {
	auto ret = new DecorPopup();
	if (ret->init()) {
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool DecorPopup::init() {
	if (!Popup::init(320.f, 200.f)) return false;

	// Estilo minimalista oscuro
	m_bgSprite->setColor({18, 18, 26});
	m_bgSprite->setOpacity(238);

	auto size = m_mainLayer->getContentSize();

	auto title = CCLabelBMFont::create("Decorador IA", "bigFont.fnt");
	title->setScale(0.55f);
	title->setPosition({size.width / 2, size.height - 24.f});
	m_mainLayer->addChild(title);

	// Cantidad de objetos seleccionados
	size_t count = 0;
	if (auto editor = EditorUI::get()) {
		if (auto sel = editor->getSelectedObjects()) count = sel->count();
	}
	m_info = CCLabelBMFont::create(
		fmt::format("{} objetos seleccionados", count).c_str(), "chatFont.fnt");
	m_info->setScale(0.7f);
	m_info->setColor(count ? COL_OK : COL_DIM);
	m_info->setPosition({size.width / 2, size.height - 50.f});
	m_mainLayer->addChild(m_info);

	// Caja de texto
	m_input = TextInput::create(280.f, "Ej: decoralo tipo demoniaco", "chatFont.fnt");
	m_input->setMaxCharCount(200);
	m_input->setPosition({size.width / 2, size.height - 95.f});
	m_mainLayer->addChild(m_input);

	m_status = CCLabelBMFont::create("Describe el estilo y pulsa Generar", "chatFont.fnt");
	m_status->setScale(0.6f);
	m_status->setColor(COL_DIM);
	m_status->setPosition({size.width / 2, 62.f});
	m_mainLayer->addChild(m_status);

	// Botones
	auto menu = CCMenu::create();
	menu->setPosition({0, 0});
	m_mainLayer->addChild(menu);

	auto closeSpr = ButtonSprite::create("Cerrar", 80, true, "bigFont.fnt", "GJ_button_06.png", 28.f, 0.6f);
	auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(DecorPopup::onCloseBtn));
	closeBtn->setPosition({size.width / 2 - 62.f, 28.f});
	menu->addChild(closeBtn);

	auto sendSpr = ButtonSprite::create("Generar", 90, true, "bigFont.fnt", "GJ_button_01.png", 28.f, 0.6f);
	m_sendBtn = CCMenuItemSpriteExtra::create(sendSpr, this, menu_selector(DecorPopup::onSend));
	m_sendBtn->setPosition({size.width / 2 + 62.f, 28.f});
	menu->addChild(m_sendBtn);

	return true;
}

void DecorPopup::onCloseBtn(CCObject*) {
	this->keyBackClicked();
}

void DecorPopup::setStatus(std::string const& text, ccColor3B color) {
	m_status->setString(text.c_str());
	m_status->setColor(color);
}

void DecorPopup::onSend(CCObject*) {
	if (m_busy) return;

	std::string prompt = m_input->getString();
	if (prompt.empty()) return this->setStatus("Escribe una referencia de decoracion", COL_ERR);

	auto editor = EditorUI::get();
	auto lel = LevelEditorLayer::get();
	if (!editor || !lel) return;

	auto sel = editor->getSelectedObjects();
	if (!sel || sel->count() == 0) return this->setStatus("Selecciona objetos en el editor primero", COL_ERR);

	auto* mod = Mod::get();

	// IDs permitidos
	m_allowed.clear();
	std::string idsRaw = mod->getSettingValue<std::string>("allowed-ids");
	size_t pos = 0;
	while (pos <= idsRaw.size()) {
		size_t comma = idsRaw.find(',', pos);
		if (comma == std::string::npos) comma = idsRaw.size();
		std::string part = idsRaw.substr(pos, comma - pos);
		part.erase(std::remove_if(part.begin(), part.end(), [](unsigned char c) { return std::isspace(c); }), part.end());
		if (!part.empty()) {
			try { m_allowed.insert(std::stoi(part)); } catch (...) {}
		}
		pos = comma + 1;
	}
	if (m_allowed.empty()) return this->setStatus("La lista de IDs permitidos esta vacia", COL_ERR);

	// Describir la seleccion (max. 120 objetos) y calcular limites
	float minX = std::numeric_limits<float>::max(), minY = minX;
	float maxX = std::numeric_limits<float>::lowest(), maxY = maxX;
	std::vector<matjson::Value> objs;
	for (auto obj : CCArrayExt<GameObject*>(sel)) {
		auto p = obj->getPosition();
		minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
		minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
		if (objs.size() < 120) {
			matjson::Value o;
			o["id"] = static_cast<int>(obj->m_objectID);
			o["x"] = static_cast<double>(p.x);
			o["y"] = static_cast<double>(p.y);
			o["rot"] = static_cast<double>(obj->getRotation());
			objs.push_back(o);
		}
	}
	float margin = 90.f;
	m_bounds = CCRect(minX - margin, minY - margin, (maxX - minX) + margin * 2, (maxY - minY) + margin * 2);

	int maxObjects = static_cast<int>(mod->getSettingValue<int64_t>("max-objects"));

	std::string allowedStr;
	for (int id : m_allowed) allowedStr += (allowedStr.empty() ? "" : ",") + std::to_string(id);

	std::string system = fmt::format(
		"You are a Geometry Dash level decorator. You receive the selected editor objects as JSON "
		"(id, x, y, rot; one block is 30 units wide) and a style request. Reply with ONLY a JSON object, "
		"no markdown, in this exact shape: "
		"{{\"objects\":[{{\"id\":int,\"x\":number,\"y\":number,\"rot\":number,\"scale\":number}}]}}. "
		"Rules: use ONLY these object ids: [{}]. Return at most {} objects. Place them inside or around the "
		"selected structure within x:[{:.0f},{:.0f}] y:[{:.0f},{:.0f}]. Align to a 15-unit grid. "
		"Make a coherent composition that matches the requested style (patterns, symmetry, layering).",
		allowedStr, maxObjects, m_bounds.getMinX(), m_bounds.getMaxX(), m_bounds.getMinY(), m_bounds.getMaxY());

	std::string user = fmt::format("Style: {}\nSelected objects: {}", prompt, matjson::Value(objs).dump(0));

	m_provider = mod->getSettingValue<std::string>("provider");

	geode::utils::web::WebRequest req;
	std::string url;
	matjson::Value body;

	if (m_provider == "Gemini") {
		std::string key = loadKey("gemini-key", "gemini-key.txt");
		if (key.empty()) return this->setStatus("Falta la API key de Gemini (ver el aviso)", COL_ERR);
		url = fmt::format("https://generativelanguage.googleapis.com/v1beta/models/{}:generateContent",
			mod->getSettingValue<std::string>("gemini-model"));
		req.header("x-goog-api-key", key);

		matjson::Value sysPart; sysPart["text"] = system;
		matjson::Value sysInst; sysInst["parts"] = std::vector<matjson::Value>{sysPart};
		matjson::Value userPart; userPart["text"] = user;
		matjson::Value content; content["role"] = "user"; content["parts"] = std::vector<matjson::Value>{userPart};
		matjson::Value gen; gen["responseMimeType"] = "application/json"; gen["maxOutputTokens"] = 8192;

		body["systemInstruction"] = sysInst;
		body["contents"] = std::vector<matjson::Value>{content};
		body["generationConfig"] = gen;
	} else if (m_provider == "Ollama") {
		std::string base = mod->getSettingValue<std::string>("ollama-url");
		while (!base.empty() && base.back() == '/') base.pop_back();
		url = base + "/api/chat";

		matjson::Value sysMsg; sysMsg["role"] = "system"; sysMsg["content"] = system;
		matjson::Value userMsg; userMsg["role"] = "user"; userMsg["content"] = user;

		body["model"] = mod->getSettingValue<std::string>("ollama-model");
		body["messages"] = std::vector<matjson::Value>{sysMsg, userMsg};
		body["stream"] = false;
		body["format"] = "json";
	} else {
		std::string key = loadKey("anthropic-key", "anthropic-key.txt");
		if (key.empty()) return this->setStatus("Falta la API key de Anthropic (ver el aviso)", COL_ERR);
		url = "https://api.anthropic.com/v1/messages";
		req.header("x-api-key", key);
		req.header("anthropic-version", "2023-06-01");

		matjson::Value msg; msg["role"] = "user"; msg["content"] = user;
		body["model"] = mod->getSettingValue<std::string>("anthropic-model");
		body["max_tokens"] = 4096;
		body["system"] = system;
		body["messages"] = std::vector<matjson::Value>{msg};
	}

	req.bodyJSON(body);
	req.timeout(std::chrono::seconds(m_provider == "Ollama" ? 240 : 120));

	m_busy = true;
	this->setStatus(fmt::format("Pensando la decoracion ({})...", m_provider), COL_DIM);

	// Geode v5: async::TaskHolder reemplaza a EventListener<WebTask>.
	// Al cerrar el popup, el holder cancela la peticion automaticamente.
	m_task.spawn(req.post(url), [this](geode::utils::web::WebResponse res) {
		this->handleResponse(res);
	});
}

static std::string extractText(std::string const& provider, matjson::Value const& json) {
	std::string text;
	if (provider == "Gemini") {
		auto cands = asVec(json["candidates"]);
		if (!cands.empty()) {
			auto parts = asVec(cands[0]["content"]["parts"]);
			for (auto const& part : parts) {
				if (auto t = part["text"].asString(); t.isOk()) text += t.unwrap();
			}
		}
	} else if (provider == "Ollama") {
		if (auto t = json["message"]["content"].asString(); t.isOk()) text = t.unwrap();
	} else {
		for (auto const& item : asVec(json["content"])) {
			if (auto t = item["text"].asString(); t.isOk()) text += t.unwrap();
		}
	}
	return text;
}

void DecorPopup::handleResponse(geode::utils::web::WebResponse& res) {
	m_busy = false;
	if (!res.ok()) {
		int code = res.code();
		if (code == 429) return this->setStatus("Limite alcanzado, espera un momento y reintenta", COL_ERR);
		if (code <= 0) return this->setStatus("No se pudo conectar (Ollama encendido?)", COL_ERR);

		// Leer el mensaje real de error que devuelve el proveedor
		std::string detail;
		auto ej = res.json().unwrapOr(matjson::Value());
		if (auto m = ej["error"]["message"].asString(); m.isOk()) detail = m.unwrap();
		else if (auto m2 = ej["error"].asString(); m2.isOk()) detail = m2.unwrap();

		this->setStatus(fmt::format("Error {} (toca OK para ver el detalle)", code), COL_ERR);
		if (detail.empty()) detail = "El servidor no devolvio detalles. Revisa la key y el modelo en los ajustes.";
		log::error("Error {} ({}): {}", code, m_provider, detail);
		FLAlertLayer::create(fmt::format("Error {} ({})", code, m_provider).c_str(), detail, "OK")->show();
		return;
	}

	auto json = res.json().unwrapOr(matjson::Value());
	auto text = extractText(m_provider, json);
	if (text.empty()) return this->setStatus("La IA devolvio una respuesta vacia", COL_ERR);

	this->applyDecoration(text);
}

void DecorPopup::applyDecoration(std::string const& text) {
	auto a = text.find('{');
	auto b = text.rfind('}');
	if (a == std::string::npos || b == std::string::npos || b < a) {
		return this->setStatus("Respuesta de la IA no valida", COL_ERR);
	}

	auto parsed = matjson::parse(text.substr(a, b - a + 1));
	if (parsed.isErr()) return this->setStatus("No pude leer el JSON de la IA", COL_ERR);

	auto root = parsed.unwrap();
	auto list = asVec(root["objects"]);

	auto lel = LevelEditorLayer::get();
	if (!lel) return;

	int maxObjects = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-objects"));
	int created = 0;

	for (auto const& item : list) {
		if (created >= maxObjects) break;

		int id = static_cast<int>(item["id"].asInt().unwrapOr(0));
		if (!m_allowed.contains(id)) continue;

		float x = static_cast<float>(item["x"].asDouble().unwrapOr(0.0));
		float y = static_cast<float>(item["y"].asDouble().unwrapOr(0.0));
		float rot = static_cast<float>(item["rot"].asDouble().unwrapOr(0.0));
		float scale = static_cast<float>(item["scale"].asDouble().unwrapOr(1.0));

		x = std::clamp(x, m_bounds.getMinX(), m_bounds.getMaxX());
		y = std::clamp(y, m_bounds.getMinY(), m_bounds.getMaxY());
		scale = std::clamp(scale, 0.2f, 3.0f);

		auto obj = lel->createObject(id, ccp(x, y), false);
		if (!obj) continue;

		obj->setRotation(rot);
		if (std::abs(scale - 1.f) > 0.01f) {
			obj->updateCustomScaleX(scale);
			obj->updateCustomScaleY(scale);
		}
		created++;
	}

	if (created == 0) this->setStatus("La IA no devolvio objetos validos", COL_ERR);
	else this->setStatus(fmt::format("Listo: {} objetos anadidos", created), COL_OK);
}

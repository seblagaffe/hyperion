// Local-Hyperion includes
#include "LedDevicePhilipsHue.h"

// jsoncpp includes
#include <json/json.h>

// qt includes
#include <QtCore/qmath.h>
#include <QEventLoop>
#include <QNetworkReply>

#include <stdexcept>

const CiColor CiColor::BLACK = { 0, 0, 0 };

bool operator ==(CiColor p1, CiColor p2) {
	return (p1.x == p2.x) && (p1.y == p2.y) && (p1.bri == p2.bri);
}

bool operator !=(CiColor p1, CiColor p2) {
	return !(p1 == p2);
}

CiColor CiColor::rgbToCiColor(float red, float green, float blue, CiColorTriangle colorSpace) {
	// Apply gamma correction.
	float r = (red > 0.04045f) ? powf((red + 0.055f) / (1.0f + 0.055f), 2.4f) : (red / 12.92f);
	float g = (green > 0.04045f) ? powf((green + 0.055f) / (1.0f + 0.055f), 2.4f) : (green / 12.92f);
	float b = (blue > 0.04045f) ? powf((blue + 0.055f) / (1.0f + 0.055f), 2.4f) : (blue / 12.92f);
	// Convert to XYZ space.
	float X = r * 0.664511f + g * 0.154324f + b * 0.162028f;
	float Y = r * 0.283881f + g * 0.668433f + b * 0.047685f;
	float Z = r * 0.000088f + g * 0.072310f + b * 0.986039f;
	// Convert to x,y space.
	float cx = X / (X + Y + Z);
	float cy = Y / (X + Y + Z);
	if (std::isnan(cx)) {
		cx = 0.0f;
	}
	if (std::isnan(cy)) {
		cy = 0.0f;
	}
	// Brightness is simply Y in the XYZ space.
	CiColor xy = { cx, cy, Y };
	// Check if the given XY value is within the color reach of our lamps.
	if (!isPointInLampsReach(xy, colorSpace)) {
		// It seems the color is out of reach let's find the closes color we can produce with our lamp and send this XY value out.
		CiColor pAB = getClosestPointToPoint(colorSpace.red, colorSpace.green, xy);
		CiColor pAC = getClosestPointToPoint(colorSpace.blue, colorSpace.red, xy);
		CiColor pBC = getClosestPointToPoint(colorSpace.green, colorSpace.blue, xy);
		// Get the distances per point and see which point is closer to our Point.
		float dAB = getDistanceBetweenTwoPoints(xy, pAB);
		float dAC = getDistanceBetweenTwoPoints(xy, pAC);
		float dBC = getDistanceBetweenTwoPoints(xy, pBC);
		float lowest = dAB;
		CiColor closestPoint = pAB;
		if (dAC < lowest) {
			lowest = dAC;
			closestPoint = pAC;
		}
		if (dBC < lowest) {
			lowest = dBC;
			closestPoint = pBC;
		}
		// Change the xy value to a value which is within the reach of the lamp.
		xy.x = closestPoint.x;
		xy.y = closestPoint.y;
	}
	return xy;
}

float CiColor::crossProduct(CiColor p1, CiColor p2) {
	return p1.x * p2.y - p1.y * p2.x;
}

bool CiColor::isPointInLampsReach(CiColor p, CiColorTriangle colorSpace) {
	CiColor v1 = { colorSpace.green.x - colorSpace.red.x, colorSpace.green.y - colorSpace.red.y };
	CiColor v2 = { colorSpace.blue.x - colorSpace.red.x, colorSpace.blue.y - colorSpace.red.y };
	CiColor q = { p.x - colorSpace.red.x, p.y - colorSpace.red.y };
	float s = crossProduct(q, v2) / crossProduct(v1, v2);
	float t = crossProduct(v1, q) / crossProduct(v1, v2);
	if ((s >= 0.0f) && (t >= 0.0f) && (s + t <= 1.0f)) {
		return true;
	}
	return false;
}

CiColor CiColor::getClosestPointToPoint(CiColor a, CiColor b, CiColor p) {
	CiColor AP = { p.x - a.x, p.y - a.y };
	CiColor AB = { b.x - a.x, b.y - a.y };
	float ab2 = AB.x * AB.x + AB.y * AB.y;
	float ap_ab = AP.x * AB.x + AP.y * AB.y;
	float t = ap_ab / ab2;
	if (t < 0.0f) {
		t = 0.0f;
	} else if (t > 1.0f) {
		t = 1.0f;
	}
	return {a.x + AB.x * t, a.y + AB.y * t};
}

float CiColor::getDistanceBetweenTwoPoints(CiColor p1, CiColor p2) {
	// Horizontal difference.
	float dx = p1.x - p2.x;
	// Vertical difference.
	float dy = p1.y - p2.y;
	// Absolute value.
	return sqrt(dx * dx + dy * dy);
}

PhilipsHueBridge::PhilipsHueBridge(QString host, QString username) :
		host(host), username(username) {
	manager = new QNetworkAccessManager();
}

PhilipsHueBridge::~PhilipsHueBridge() {
	delete manager;
}

QByteArray PhilipsHueBridge::get(QString route) {
	// Perfrom request
	QNetworkRequest request(QString("http://%1/api/%2/%3").arg(host).arg(username).arg(route));
	QNetworkReply* reply = manager->get(request);
	// Connect requestFinished signal to quit slot of the loop.
	QEventLoop loop;
	loop.connect(reply, SIGNAL(finished()), SLOT(quit()));
	// Go into the loop until the request is finished.
	loop.exec();
	// Read all data of the response.
	QByteArray response = reply->readAll();
	// Free space.
	reply->deleteLater();
	// Return response;
	return response;
}

void PhilipsHueBridge::post(QString route, QString content) {
	// Perfrom request
	QNetworkRequest request(QString("http://%1/api/%2/%3").arg(host).arg(username).arg(route));
	QNetworkReply* reply = manager->put(request, content.toLatin1());
	// Connect finished signal to quit slot of the loop.
	QEventLoop loop;
	loop.connect(reply, SIGNAL(finished()), SLOT(quit()));
	// Go into the loop until the request is finished.
	loop.exec();
	// Free space.
	reply->deleteLater();
}

const std::set<QString> PhilipsHueLight::GAMUT_A_MODEL_IDS = { "LLC001", "LLC005", "LLC006", "LLC007", "LLC010",
		"LLC011", "LLC012", "LLC013", "LLC014", "LST001" };
const std::set<QString> PhilipsHueLight::GAMUT_B_MODEL_IDS = { "LCT001", "LCT002", "LCT003", "LCT007", "LLM001" };
const std::set<QString> PhilipsHueLight::GAMUT_C_MODEL_IDS = { "LLC020", "LST002" };

PhilipsHueLight::PhilipsHueLight(PhilipsHueBridge& bridge, unsigned int id) :
		bridge(bridge), id(id) {
	// Get model id and original state.
	QByteArray response = bridge.get(QString("lights/%1").arg(id));
	// Use JSON parser to parse response.
	Json::Reader reader;
	Json::FastWriter writer;
	// Parse response.
	Json::Value json;
	if (!reader.parse(QString(response).toStdString(), json)) {
		std::runtime_error(QString("Got invalid response from light %1").arg(id).toStdString());
	}
	// Get state object values which are subject to change.
	Json::Value state(Json::objectValue);
	if (!json.isMember("state")) {
		std::runtime_error(QString("Got no state object from light %1").arg(id).toStdString());
	}
	if (!json["state"].isMember("on")) {
		std::runtime_error(QString("Got invalid state object from light %1").arg(id).toStdString());
	}
	state["on"] = json["state"]["on"];
	on = false;
	if (json["state"]["on"] == true) {
		state["xy"] = json["state"]["xy"];
		state["bri"] = json["state"]["bri"];
		on = true;
		color = {state["xy"][0].asFloat(), state["xy"][1].asFloat(), state["bri"].asFloat() / 255.0f};
		transitionTime = json["state"]["transitiontime"].asInt();
	}
	// Determine the model id.
	modelId = QString(writer.write(json["modelid"]).c_str()).trimmed().replace("\"", "");
	// Determine the original state.
	originalState = QString(writer.write(state).c_str()).trimmed();
	// Find id in the sets and set the appropriate color space.
	if (GAMUT_A_MODEL_IDS.find(modelId) != GAMUT_A_MODEL_IDS.end()) {
		colorSpace.red = {0.703f, 0.296f};
		colorSpace.green = {0.2151f, 0.7106f};
		colorSpace.blue = {0.138f, 0.08f};
	} else if (GAMUT_B_MODEL_IDS.find(modelId) != GAMUT_B_MODEL_IDS.end()) {
		colorSpace.red = {0.675f, 0.322f};
		colorSpace.green = {0.4091f, 0.518f};
		colorSpace.blue = {0.167f, 0.04f};
	} else if (GAMUT_C_MODEL_IDS.find(modelId) != GAMUT_B_MODEL_IDS.end()) {
		colorSpace.red = {0.675f, 0.322f};
		colorSpace.green = {0.2151f, 0.7106f};
		colorSpace.blue = {0.167f, 0.04f};
	} else {
		colorSpace.red = {1.0f, 0.0f};
		colorSpace.green = {0.0f, 1.0f};
		colorSpace.blue = {0.0f, 0.0f};
	}
}

PhilipsHueLight::~PhilipsHueLight() {
	// Restore the original state.
	set(originalState);
}

void PhilipsHueLight::set(QString state) {
	bridge.post(QString("lights/%1/state").arg(id), state);
}

void PhilipsHueLight::setOn(bool on, bool force) {
	if (force || this->on != on) {
		QString arg = on ? "true" : "false";
		set(QString("{ \"on\": %1 }").arg(arg));
	}
	this->on = on;
}

bool PhilipsHueLight::isOn() const {
	return on;
}

void PhilipsHueLight::setTransitionTime(unsigned int transitionTime, bool force) {
	if (force || this->transitionTime != transitionTime) {
		set(QString("{ \"transitiontime\": %1 }").arg(transitionTime));
	}
	this->transitionTime = transitionTime;
}

unsigned int PhilipsHueLight::getTransitionTime() const {
	return transitionTime;
}

void PhilipsHueLight::setColor(CiColor color, bool setBrightness, bool force) {
	if (force || this->color != color) {
		setOn(true);
		if (setBrightness) {
			set(
					QString("{ \"xy\": [%1, %2], \"bri\": %3 }").arg(color.x, 0, 'f', 4).arg(color.y, 0, 'f', 4).arg(
							color.bri * 254));
		} else {
			set(QString("{ \"xy\": [%1, %2], \"bri\": 254 }").arg(color.x, 0, 'f', 4).arg(color.y, 0, 'f', 4));
		}
	}
	this->color = color;
}

CiColor PhilipsHueLight::getColor() const {
	return color;
}

CiColorTriangle PhilipsHueLight::getColorSpace() const {
	return colorSpace;
}

LedDevicePhilipsHue::LedDevicePhilipsHue(const std::string& output, const std::string& username, bool switchOffOnBlack,
		bool setBrightness, int transitionTime, std::vector<unsigned int> lightIds) :
		bridge( { output.c_str(), username.c_str() }), switchOffOnBlack(switchOffOnBlack), setBrightness(setBrightness), transitionTime(
				transitionTime), lightIds(lightIds) {
	timer.setInterval(3000);
	timer.setSingleShot(true);
	connect(&timer, SIGNAL(timeout()), this, SLOT(restoreStates()));
}

LedDevicePhilipsHue::~LedDevicePhilipsHue() {
	// Switch off.
	switchOff();
}

int LedDevicePhilipsHue::write(const std::vector<ColorRgb> & ledValues) {
	// Save light states if not done before.
	if (!areStatesSaved()) {
		saveStates((unsigned int) ledValues.size());
	}
	// If there are less states saved than colors given, then maybe something went wrong beforeno.
	if (lights.size() != ledValues.size()) {
		restoreStates();
		return 0;
	}
	// Iterate through colors and set light states.
	unsigned int idx = 0;
	for (const ColorRgb& color : ledValues) {
		// Get lamp.
		PhilipsHueLight& light = lights.at(idx);
		// Scale colors from [0, 255] to [0, 1] and convert to xy space.
		CiColor xy = CiColor::rgbToCiColor(color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f, light.getColorSpace());
		// Write color if color has been changed.
		if (switchOffOnBlack && light.getColor() != CiColor::BLACK && xy == CiColor::BLACK) {
			light.setOn(false);
		} else if (switchOffOnBlack && light.getColor() == CiColor::BLACK && xy != CiColor::BLACK) {
			light.setOn(true);
		}
		light.setOn(true);
		light.setColor(xy, setBrightness);
		// Next light id.
		idx++;
	}
	// Reset timer.
	timer.start();
	return 0;
}

int LedDevicePhilipsHue::switchOff() {
	timer.stop();
	// If light states have been saved before, ...
	if (areStatesSaved()) {
		// ... restore them.
		restoreStates();
	}
	return 0;
}

void LedDevicePhilipsHue::saveStates(unsigned int nLights) {
	// Clear saved lamps.
	lights.clear();
	// Use JSON parser to parse reponse.
	Json::Reader reader;
	// Read light ids if none have been supplied by the user.
	if (lightIds.size() != nLights) {
		lightIds.clear();
		// Retrieve lights from bridge.
		QByteArray response = bridge.get("lights");
		Json::Value json;
		if (!reader.parse(QString(response).toStdString(), json)) {
			throw std::runtime_error("No lights found.");
		}
		// Loop over all children.
		for (Json::ValueIterator it = json.begin(); it != json.end() && lightIds.size() < nLights; it++) {
			int lightId = atoi(it.key().asCString());
			lightIds.push_back(lightId);
			std::cout << "LedDevicePhilipsHue::saveStates(nLights=" << nLights << "): found light with id " << lightId
					<< "." << std::endl;
		}
		// Check if we found enough lights.
		if (lightIds.size() != nLights) {
			throw std::runtime_error("Not enough lights found");
		}
	}
	// Iterate lights.
	for (unsigned int i = 0; i < nLights; i++) {
		lights.push_back(PhilipsHueLight(bridge, lightIds.at(i)));
	}
}

void LedDevicePhilipsHue::restoreStates() {
	lights.clear();
}

bool LedDevicePhilipsHue::areStatesSaved() {
	return !lights.empty();
}

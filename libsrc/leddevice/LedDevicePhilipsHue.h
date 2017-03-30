#pragma once

// STL includes
#include <string>
#include <set>

// Qt includes
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QTimer>
// Leddevice includes
#include <leddevice/LedDevice.h>

// Forward declaration
struct CiColorTriangle;

/**
 * A color point in the color space of the hue system.
 */
struct CiColor {
	/// X component.
	float x;
	/// Y component.
	float y;
	/// The brightness.
	float bri;
	/// Black color constant.
	static const CiColor BLACK;

	///
	/// Converts an RGB color to the Hue xy color space and brightness.
	/// https://github.com/PhilipsHue/PhilipsHueSDK-iOS-OSX/blob/master/ApplicationDesignNotes/RGB%20to%20xy%20Color%20conversion.md
	///
	/// @param red the red component in [0, 1]
	///
	/// @param green the green component in [0, 1]
	///
	/// @param blue the blue component in [0, 1]
	///
	/// @return color point
	///
	static CiColor rgbToCiColor(float red, float green, float blue, CiColorTriangle colorSpace);

	///
	/// @param p the color point to check
	///
	/// @return true if the color point is covered by the lamp color space
	///
	static bool isPointInLampsReach(CiColor p, CiColorTriangle colorSpace);

	///
	/// @param p1 point one
	///
	/// @param p2 point tow
	///
	/// @return the cross product between p1 and p2
	///
	static float crossProduct(CiColor p1, CiColor p2);

	///
	/// @param a reference point one
	///
	/// @param b reference point two
	///
	/// @param p the point to which the closest point is to be found
	///
	/// @return the closest color point of p to a and b
	///
	static CiColor getClosestPointToPoint(CiColor a, CiColor b, CiColor p);

	///
	/// @param p1 point one
	///
	/// @param p2 point tow
	///
	/// @return the distance between the two points
	///
	static float getDistanceBetweenTwoPoints(CiColor p1, CiColor p2);
};

bool operator==(CiColor p1, CiColor p2);
bool operator!=(CiColor p1, CiColor p2);

/**
 * Color triangle to define an available color space for the hue lamps.
 */
struct CiColorTriangle {
	CiColor red, green, blue;
};

class PhilipsHueBridge {
private:
	/// QNetworkAccessManager object for sending requests.
	QNetworkAccessManager* manager;
	/// Ip address of the bridge
	QString host;
	/// User name for the API ("newdeveloper")
	QString username;

public:
	PhilipsHueBridge(QString host, QString username);
	~PhilipsHueBridge();

	///
	/// @param route the route of the GET request.
	///
	/// @return the response of the GET request.
	///
	QByteArray get(QString route);

	///
	/// @param route the route of the POST request.
	///
	/// @param content the content of the POST request.
	///
	void post(QString route, QString content);
};

/**
 * Simple class to hold the id, the latest color, the color space and the original state.
 */
class PhilipsHueLight {
private:
	PhilipsHueBridge& bridge;
	unsigned int id;
	bool on;
	unsigned int transitionTime;
	CiColor color;
	/// The model id of the hue lamp which is used to determine the color space.
	QString modelId;
	CiColorTriangle colorSpace;
	/// The json string of the original state.
	QString originalState;

	///
	/// @param state the state as json object to set
	///
	void set(QString state);

public:
	// Hue system model ids (http://www.developers.meethue.com/documentation/supported-lights).
	// Light strips, color iris, ...
	static const std::set<QString> GAMUT_A_MODEL_IDS;
	// Hue bulbs, spots, ...
	static const std::set<QString> GAMUT_B_MODEL_IDS;
	// Hue Lightstrip plus, go ...
	static const std::set<QString> GAMUT_C_MODEL_IDS;

	///
	/// Constructs the light.
	///
	/// @param bridge the bridge
	///
	/// @param id the light id
	///
	PhilipsHueLight(PhilipsHueBridge& bridge, unsigned int id);
	~PhilipsHueLight();

	///
	/// @param on
	///
	void setOn(bool on);

	///
	/// @param transitionTime the transition time between colors in multiples of 100 ms
	///
	void setTransitionTime(unsigned int transitionTime);

	///
	/// @param color the color to set
	/// @param brightnessFactor the factor to apply to the CiColor#bri value
	///
	void setColor(CiColor color, float brightnessFactor = 1.0f);
	CiColor getColor() const;

	///
	/// @return the color space of the light determined by the model id reported by the bridge.
	CiColorTriangle getColorSpace() const;

};

/**
 * Implementation for the Philips Hue system.
 *
 * To use set the device to "philipshue".
 * Uses the official Philips Hue API (http://developers.meethue.com).
 * Framegrabber must be limited to 10 Hz / numer of lights to avoid rate limitation by the hue bridge.
 * Create a new API user name "newdeveloper" on the bridge (http://developers.meethue.com/gettingstarted.html)
 *
 * @author ntim (github), bimsarck (github)
 */
class LedDevicePhilipsHue: public QObject, public LedDevice {
	Q_OBJECT
public:
	///
	/// Constructs the device.
	///
	/// @param output the ip address of the bridge
	///
	/// @param username username of the hue bridge (default: newdeveloper)
	///
	/// @param switchOffOnBlack kill lights for black (default: false)
	///
	/// @param brightnessFactor set the brightness factor to multiply (default: 1.0)
	///
	/// @param transitionTime the time duration a light change takes in multiples of 100 ms (default: 400 ms).
	///
	/// @param lightIds light ids of the lights to control if not starting at one in ascending order.
	///
	LedDevicePhilipsHue(const std::string& output, const std::string& username = "newdeveloper", bool switchOffOnBlack =
			false, float brightnessFactor = 1.0, int transitionTime = 1,
			std::vector<unsigned int> lightIds = std::vector<unsigned int>());

	///
	/// Destructor of this device
	///
	virtual ~LedDevicePhilipsHue();

	///
	/// Sends the given led-color values via put request to the hue system
	///
	/// @param ledValues The color-value per led
	///
	/// @return Zero on success else negative
	///
	virtual int write(const std::vector<ColorRgb> & ledValues);

	/// Restores the original state of the leds.
	virtual int switchOff();

private slots:
	/// Restores the status of all lights.
	void restoreStates();

private:
	PhilipsHueBridge bridge;
	/// Use timer to reset lights when we got into "GRABBINGMODE_OFF".
	QTimer timer;
	///
	bool switchOffOnBlack;
	/// The brightness factor to multiply on color change.
	float brightnessFactor;
	/// Transition time in multiples of 100 ms.
	/// The default of the Hue lights is 400 ms, but we may want it snapier.
	int transitionTime;
	/// Array of the light ids.
	std::vector<unsigned int> lightIds;
	/// Array to save the lamps.
	std::vector<PhilipsHueLight> lights;

	///
	/// Queries the status of all lights and saves it.
	///
	/// @param nLights the number of lights
	///
	void saveStates(unsigned int nLights);

	///
	/// @return true if light states have been saved.
	///
	bool areStatesSaved();

};

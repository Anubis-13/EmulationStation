#pragma once
#ifndef ES_CORE_SETTINGS_H
#define ES_CORE_SETTINGS_H

#include <map>

//This is a singleton for storing settings.
class Settings
{
public:
	static Settings* getInstance();

	void loadFile();
	void saveFile();

	//You will get a warning if you try a get on a key that is not already present.
	bool getBool(const std::string& name);
	int getInt(const std::string& name);
	float getFloat(const std::string& name);
	const std::string& getString(const std::string& name);

	void setBool(const std::string& name, bool value);
	void setInt(const std::string& name, int value);
	void setFloat(const std::string& name, float value);
	void setString(const std::string& name, const std::string& value);

private:
	static Settings* sInstance;

	Settings();

	//Clear everything and load default values.
	void setDefaults();

	std::map<std::string, bool> mBoolMap;
	std::map<std::string, int> mIntMap;
	std::map<std::string, float> mFloatMap;
	std::map<std::string, std::string> mStringMap;


	std::map<std::string, bool> mDefaultBoolMap;
	std::map<std::string, int> mDefaultIntMap;
	std::map<std::string, float> mDefaultFloatMap;
	std::map<std::string, std::string> mDefaultStringMap;
};

#endif // ES_CORE_SETTINGS_H

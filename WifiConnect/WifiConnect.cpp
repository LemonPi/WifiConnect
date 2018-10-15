// WifiConnect.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <fstream>
#include <Windows.h>
#include <wlanapi.h>
#include <ShlObj.h>
#include <string>
#include <unordered_map>
#pragma comment(lib, "Wlanapi.lib")

std::wstring getProgramUserPath(const wchar_t* programName, const wchar_t* extension) {
	LPWSTR path = NULL;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DEFAULT, NULL, &path))) {
		return {};
	}

	auto p = std::wstring(path) + L'\\' + programName;
	if (extension != nullptr) {
		p += extension;
	}

	return p;
}

std::string stateToString(WLAN_INTERFACE_STATE state) {
	static const std::unordered_map<WLAN_INTERFACE_STATE, std::string> map({
		{wlan_interface_state_not_ready, "Not ready"},
		{wlan_interface_state_connected, "Connected"},
		{wlan_interface_state_ad_hoc_network_formed, "First node in an ad hoc netowrk"},
		{wlan_interface_state_disconnecting, "Disconnecting"},
		{wlan_interface_state_disconnected, "Not connected"},
		{wlan_interface_state_associating, "Attempting to associate with a network"},
		{wlan_interface_state_discovering, "Auto configuration is discovering settings for the network"},
		{wlan_interface_state_authenticating, "In process of authenticating"}
		});

	const auto foundState = map.find(state);
	if (foundState == map.end()) {
		return "Unknown state";
	}
	return foundState->second;
}

void printInterfaces(PWLAN_INTERFACE_INFO_LIST pIfList, std::ostream& log) {
	log << "Num entires: " << pIfList->dwNumberOfItems << std::endl;
	log << "Current index: " << pIfList->dwIndex << std::endl;

	constexpr auto GUID_LENGTH = 39;
	WCHAR guidString[GUID_LENGTH + 1] = { 0 };
	for (int i = 0; i < (int)pIfList->dwNumberOfItems; ++i) {
		const auto* info = (WLAN_INTERFACE_INFO*)&pIfList->InterfaceInfo[i];
		log << "-------------------------------------\n";
		log << "Interface index: " << i << std::endl;

		auto innerResult = StringFromGUID2(info->InterfaceGuid, (LPOLESTR)&guidString, GUID_LENGTH);
		if (innerResult == 0)
			log << "StringFromGUID2 failed\n";
		else {
			log << "InterfaceGUID: " << guidString << std::endl;
		}
		log << "Interface description: " << info->strInterfaceDescription << std::endl;
		log << "Interface state: " << stateToString(info->isState) << std::endl;
	}
}

int main() {
	// log to file since we intend to run this program at startup so we won't see console output
	const auto logFile = getProgramUserPath(L"WifiConnect", L".txt");
	if (logFile.empty()) {
		std::cout << "Couldn't get user profile path\n";
		return 1;
	}
	// only useful during debugging
	std::wcout << "Logging to " << logFile << std::endl;

	std::ofstream log(logFile);

	HANDLE clientHandle = 0;
	DWORD clientVersion = 0;
	auto result = WlanOpenHandle(2, NULL, &clientVersion, &clientHandle);
	if (result != ERROR_SUCCESS) {
		log << "WlanOpenhandle failed with error " << result << std::endl;
		return 1;
	}

	log << "Client " << clientHandle << " version " << clientVersion << std::endl;

	PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
	result = WlanEnumInterfaces(clientHandle, NULL, &pIfList);
	if (result != ERROR_SUCCESS) {
		log << "WlanEnumInterfaces failed with error " << result << std::endl;
		return 1;
	}

	printInterfaces(pIfList, log);

	// select which interface to use if you have multiple wifi interfaces (default to first one)
	constexpr auto INTERFACE_INDEX = 0;
	const auto* info = (WLAN_INTERFACE_INFO*)&pIfList->InterfaceInfo[INTERFACE_INDEX];

	// TODO only run this if our interface isn't connected to anything
	// TODO try 2 approaches: 1) get current list of networks and connect on startup (preferred) 2) initiate scan and register notification

	//WlanScan(clientHandle, info->InterfaceGuid)
	if (pIfList != NULL) {
		WlanFreeMemory(pIfList);
	}

	WlanCloseHandle(clientHandle, NULL);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

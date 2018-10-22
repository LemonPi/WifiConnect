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
	CoTaskMemFree(path);
	if (extension != nullptr) {
		p += extension;
	}

	return p;
}

std::string currentDateTime() {
	SYSTEMTIME st;
	GetLocalTime(&st);

	std::string currentTime( 20, ' ' );
	currentTime.resize(snprintf(&currentTime[0], currentTime.size(), "%.02d/%.02d/%.04d %.02d:%.02d:%.02d",
		st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond));
	return currentTime;
}


const char * stateToString(WLAN_INTERFACE_STATE state) {
	switch (state) {
	case wlan_interface_state_not_ready:
		return "Not ready";
	case wlan_interface_state_connected:
		return "Connected";
	case wlan_interface_state_ad_hoc_network_formed:
		return "First node in an ad hoc netowrk";
	case wlan_interface_state_disconnecting:
		return "Disconnecting";
	case wlan_interface_state_disconnected:
		return "Not connected";
	case wlan_interface_state_associating:
		return "Attempting to associate with a network";
	case wlan_interface_state_discovering:
		return "Auto configuration is discovering settings for the network";
	case wlan_interface_state_authenticating:
		return "In process of authenticating";
	default:
		return "Unknown state";
	}
}

void printInterfaces(PWLAN_INTERFACE_INFO_LIST pIfList, std::ostream& log) {
	log << "Num entires: " << pIfList->dwNumberOfItems << std::endl;
	log << "Current index: " << pIfList->dwIndex << std::endl;

	constexpr auto GUID_LENGTH = 39;
	WCHAR guidString[GUID_LENGTH + 1] = { 0 };
	for (int i = 0; i < (int)pIfList->dwNumberOfItems; ++i) {
		const auto info = (WLAN_INTERFACE_INFO*)&pIfList->InterfaceInfo[i];
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

std::string getNetworkSsidString(const WLAN_AVAILABLE_NETWORK* network) {
	return std::string( ( const char * ) network->dot11Ssid.ucSSID, network->dot11Ssid.uSSIDLength);
}

void printNetworkList(PWLAN_AVAILABLE_NETWORK_LIST pBssList, std::ostream& log) {
	log << "WLAN_AVAILABLE_NETWORK_LIST for this interface\n";
	log << "Num entries: " << pBssList->dwNumberOfItems << std::endl;

	for (int j = 0; j < (int)pBssList->dwNumberOfItems; ++j) {
		log << "-----\n";
		const auto network = (WLAN_AVAILABLE_NETWORK *)& pBssList->Network[j];
		log << "Profile name: " << network->strProfileName << std::endl;

		log << "SSID: " << getNetworkSsidString(network) << std::endl;

		log << "Connectable: ";
		if (network->bNetworkConnectable) {
			log << "Yes";
		} else {
			log << "No because " << network->wlanNotConnectableReason;
		}
		log << std::endl;

		log << "Flags: " << network->dwFlags;
		if (network->dwFlags) {
			if (network->dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) {
				log << " - Currently connected";
			}
			if (network->dwFlags & WLAN_AVAILABLE_NETWORK_HAS_PROFILE) {
				log << " - Has profile";
			}
		}
		log << std::endl;

		log << "Signal quality: " << network->wlanSignalQuality << std::endl;
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

	log << currentDateTime() << std::endl;

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
	const auto info = (WLAN_INTERFACE_INFO*)&pIfList->InterfaceInfo[INTERFACE_INDEX];

	// only run this if our interface isn't connected to anything
	if (info->isState == wlan_interface_state_connected) {
		log << "Wlan interface is already connected; terminating\n";
		return 0;
	}

	// TODO try 2 approaches: 1) get current list of networks and connect on startup (preferred) 2) initiate scan and register notification
	PWLAN_AVAILABLE_NETWORK_LIST pBssList = NULL;
	result = WlanGetAvailableNetworkList(clientHandle, &info->InterfaceGuid, 0, NULL, &pBssList);
	if (result != ERROR_SUCCESS) {
		log << "WlanGetAvailableNetworkList failed with error " << result << std::endl;
		return 1;
	}

	printNetworkList(pBssList, log);
	// information for which network to connect to
	const auto targetSsid = "NETGEAR41-5G";
	// we expect it to be above a certain signal quality for us to automatically connect to it
	// 0 - 100 (see printed list for what existing network signal qualities are)
	const auto minAcceptableSignalQuality = 75;
	auto foundTargetNetwork = false;
	for (int j = 0; j < (int)pBssList->dwNumberOfItems; ++j) {
		const auto network = (WLAN_AVAILABLE_NETWORK *)& pBssList->Network[j];
		if (targetSsid == getNetworkSsidString(network)) {
			log << "Found target network with quality " << network->wlanSignalQuality << std::endl;
			foundTargetNetwork = true;

			if (network->wlanSignalQuality < minAcceptableSignalQuality) {
				log << "But it's signal quality is too low\n";
			} else {
				log << "Connecting to network using existing profile\n";
				// connect to this network
				WLAN_CONNECTION_PARAMETERS cp;
				memset(&cp, 0, sizeof(WLAN_CONNECTION_PARAMETERS));
				cp.wlanConnectionMode = wlan_connection_mode_profile;
				cp.strProfile = network->strProfileName;
				cp.dwFlags = 0;
				cp.pDot11Ssid = NULL;
				cp.pDesiredBssidList = 0;
				cp.dot11BssType = network->dot11BssType;
				result = WlanConnect(clientHandle, &info->InterfaceGuid, &cp, NULL);
				if (result != ERROR_SUCCESS) {
					log << "WlanConnect failed with error " << result << std::endl;
					return 1;
				} else {
					log << "Successfully connected\n";
				}
			}
			break;
		}
	}

	if (!foundTargetNetwork) {
		log << "Could not find target network with SSID: " << targetSsid << std::endl;
	}

	//WlanScan(clientHandle, info->InterfaceGuid)
	// seems OK to only cleanup on success based on sample code? (kind of weird...)
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

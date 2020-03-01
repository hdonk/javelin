#include "pch.h"
#include "javelin.h"

#include <../javelin_test/bin/javelin_test_javelin.h>

#include <iostream>

using namespace winrt;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation;

Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> m_knownDevices = single_threaded_observable_vector<Windows::Foundation::IInspectable>();
std::vector<Windows::Devices::Enumeration::DeviceInformation> UnknownDevices;
Windows::Devices::Enumeration::DeviceWatcher deviceWatcher{ nullptr };
event_token deviceWatcherAddedToken;
event_token deviceWatcherUpdatedToken;
event_token deviceWatcherRemovedToken;
event_token deviceWatcherEnumerationCompletedToken;
event_token deviceWatcherStoppedToken;

class Discovery
{
        static Discovery* sm_dc;

        std::map<std::wstring, Windows::Devices::Enumeration::DeviceInformation *> m_id_to_di;

    public:
        Discovery()
            : m_id_to_di()
        {

        };

        void add_di(std::wstring &ar_id, Windows::Devices::Enumeration::DeviceInformation ar_di)
        {
            m_id_to_di[ar_id] = ::new Windows::Devices::Enumeration::DeviceInformation(ar_di);
            Windows::Devices::Enumeration::DeviceInformation *lp_di = m_id_to_di[ar_id];
            std::wcout << __func__ << ":" << std::endl;
            display_di(lp_di);
        }

        void update_di(std::wstring& ar_id, Windows::Devices::Enumeration::DeviceInformationUpdate ar_diu)
        {
            m_id_to_di[ar_id]->Update(ar_diu);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_di[ar_id];
            std::wcout << __func__ << ":" << std::endl;
            display_di(lp_di);
        }

        static void display_di(Windows::Devices::Enumeration::DeviceInformation* ap_di)
        {
            std::wstring l_id(ap_di->Id().c_str());
            std::wstring l_name(ap_di->Name().c_str());
            std::wcout << "Id: " << l_id << std::endl;
            std::wcout << "Name: " << l_name << std::endl;
        }

        static Discovery* getDiscovery()
        {
            if (!sm_dc)
            {
                sm_dc = new Discovery();
            }
            return sm_dc;
        }

        static void DeviceWatcher_Added(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Devices::Enumeration::DeviceInformation deviceInfo)
        {
            std::wcout << "In " << __func__ << std::endl;
            if (!sm_dc) return;
            std::wstring l_id(deviceInfo.Id().c_str());
            sm_dc->add_di(l_id, deviceInfo);
        };
        static void DeviceWatcher_Updated(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Devices::Enumeration::DeviceInformationUpdate deviceInfoUpdate)
        {
            std::wcout << "In " << __func__ << std::endl;
            if (!sm_dc) return;
            std::wstring l_id(deviceInfoUpdate.Id().c_str());
            sm_dc->update_di(l_id, deviceInfoUpdate);
        };
        static void DeviceWatcher_Removed(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Devices::Enumeration::DeviceInformationUpdate deviceInfoUpdate)
        {
            std::wcout << "In " << __func__ << std::endl;
            if (!sm_dc) return;
        };
        static void DeviceWatcher_EnumerationCompleted(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Foundation::IInspectable const&)
        {
            std::wcout << "In " << __func__ << std::endl;
            if (!sm_dc) return;
        };
        static void DeviceWatcher_Stopped(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Foundation::IInspectable const&)
        {
            std::wcout << "In " << __func__ << std::endl;
            if (!sm_dc) return;
        };
};

Discovery *Discovery::sm_dc = NULL;

JNIEXPORT void JNICALL Java_javelin_1test_javelin_listBLEDevices
(JNIEnv*, jclass)
{
	std::cout << "Hello from JNI C++" << std::endl;

    Discovery *lp_dc = Discovery::getDiscovery();

    auto requestedProperties = single_threaded_vector<hstring>({ L"System.Devices.Aep.DeviceAddress", L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.Bluetooth.Le.IsConnectable" });
    std::cout << "1" << std::endl;
    // BT_Code: Example showing paired and non-paired in a single query.
    hstring aqsAllBluetoothLEDevices = L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";
    std::cout << "2" << std::endl;
    deviceWatcher =
        Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
            aqsAllBluetoothLEDevices,
            requestedProperties,
            DeviceInformationKind::AssociationEndpoint);
    std::cout << "3" << std::endl;
    // Register event handlers before starting the watcher.
    deviceWatcherAddedToken = deviceWatcher.Added(&Discovery::DeviceWatcher_Added );
    deviceWatcherUpdatedToken = deviceWatcher.Updated(&Discovery::DeviceWatcher_Updated );
    deviceWatcherRemovedToken = deviceWatcher.Removed(&Discovery::DeviceWatcher_Removed );
    deviceWatcherEnumerationCompletedToken = deviceWatcher.EnumerationCompleted(&Discovery::DeviceWatcher_EnumerationCompleted );
    deviceWatcherStoppedToken = deviceWatcher.Stopped(&Discovery::DeviceWatcher_Stopped );
    
    // Start over with an empty collection.
    m_knownDevices.Clear();
    std::cout << "4" << std::endl;

    // Start the watcher. Active enumeration is limited to approximately 30 seconds.
    // This limits power usage and reduces interference with other Bluetooth activities.
    // To monitor for the presence of Bluetooth LE devices for an extended period,
    // use the BluetoothLEAdvertisementWatcher runtime class. See the BluetoothAdvertisement
    // sample for an example.
    deviceWatcher.Start();
    std::cout << "5 - Sleeping" << std::endl;
    Sleep(10000);
    std::cout << "6 - Slept" << std::endl;

}
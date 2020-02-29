#include "pch.h"
#include "javelin.h"

#include "../javelin_test/bin/javelin_test_javelin.h"

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


JNIEXPORT void JNICALL Java_javelin_1test_javelin_listBLEDevices
(JNIEnv*, jclass)
{
	std::cout << "Hello from JNI C++" << std::endl;

    auto requestedProperties = single_threaded_vector<hstring>({ L"System.Devices.Aep.DeviceAddress", L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.Bluetooth.Le.IsConnectable" });

    // BT_Code: Example showing paired and non-paired in a single query.
    hstring aqsAllBluetoothLEDevices = L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";

    deviceWatcher =
        Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
            aqsAllBluetoothLEDevices,
            requestedProperties,
            DeviceInformationKind::AssociationEndpoint);

    // Register event handlers before starting the watcher.
/*    deviceWatcherAddedToken = deviceWatcher.Added({ get_weak(), &Scenario1_Discovery::DeviceWatcher_Added });
    deviceWatcherUpdatedToken = deviceWatcher.Updated({ get_weak(), &Scenario1_Discovery::DeviceWatcher_Updated });
    deviceWatcherRemovedToken = deviceWatcher.Removed({ get_weak(), &Scenario1_Discovery::DeviceWatcher_Removed });
    deviceWatcherEnumerationCompletedToken = deviceWatcher.EnumerationCompleted({ get_weak(), &Scenario1_Discovery::DeviceWatcher_EnumerationCompleted });
    deviceWatcherStoppedToken = deviceWatcher.Stopped({ get_weak(), &Scenario1_Discovery::DeviceWatcher_Stopped });
    */
    // Start over with an empty collection.
    m_knownDevices.Clear();

    // Start the watcher. Active enumeration is limited to approximately 30 seconds.
    // This limits power usage and reduces interference with other Bluetooth activities.
    // To monitor for the presence of Bluetooth LE devices for an extended period,
    // use the BluetoothLEAdvertisementWatcher runtime class. See the BluetoothAdvertisement
    // sample for an example.
    deviceWatcher.Start();

}
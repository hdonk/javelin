#include "pch.h"
#include "javelin.h"

#include <../javelin_test/bin/javelin_test_javelin.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <concrt.h>
using namespace winrt;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Foundation::Collections;
//using namespace Windows::Foundation::Collections::IVectorView;

// Utility Functions
std::wstring Java_To_WStr(JNIEnv* env, jstring string)
{
    std::wstring value;

    const jchar* raw = env->GetStringChars(string, 0);
    jsize len = env->GetStringLength(string);

    value.assign(raw, raw + len);

    env->ReleaseStringChars(string, raw);

    return value;
}


class Discovery
{
        static Discovery* sm_dc;

        concurrency::critical_section m_lock_std;
        std::map<std::wstring, Windows::Devices::Enumeration::DeviceInformation *> m_id_to_di;
        concurrency::event m_discovery_complete;

        Windows::Devices::Enumeration::DeviceWatcher m_deviceWatcher{ nullptr };
        event_token m_deviceWatcherAddedToken;
        event_token m_deviceWatcherUpdatedToken;
        event_token m_deviceWatcherRemovedToken;
        event_token m_deviceWatcherEnumerationCompletedToken;
        event_token m_deviceWatcherStoppedToken;

        GattDeviceServicesResult m_temp_gdsr;

    public:
        Discovery()
            : m_lock_std()
            , m_id_to_di()
            , m_discovery_complete()
            , m_temp_gdsr{nullptr}
        {
            m_discovery_complete.reset();
        };

        void add_di(std::wstring &ar_id, Windows::Devices::Enumeration::DeviceInformation ar_di)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            m_id_to_di[ar_id] = ::new Windows::Devices::Enumeration::DeviceInformation(ar_di);
            Windows::Devices::Enumeration::DeviceInformation *lp_di = m_id_to_di[ar_id];
            display_di(lp_di);
        }

        void update_di(std::wstring& ar_id, Windows::Devices::Enumeration::DeviceInformationUpdate ar_diu)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            m_id_to_di[ar_id]->Update(ar_diu);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_di[ar_id];
            display_di(lp_di);
        }

        void remove_di(std::wstring& ar_id)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_di[ar_id];
            display_di(lp_di);
            ::delete m_id_to_di[ar_id];
            m_id_to_di.erase(ar_id);
        }

        std::wstring getName(std::wstring& ar_id)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_di[ar_id];
            if (!lp_di) return std::wstring();
            std::wstring l_name(lp_di->Name().c_str());
            return l_name;
        }

        static void display_di(Windows::Devices::Enumeration::DeviceInformation* ap_di)
        {
            return;
/*            std::wstring l_id(ap_di->Id().c_str());
            std::wstring l_name(ap_di->Name().c_str());
            std::wcout << "Id: " << l_id << std::endl;
            std::wcout << "Name: " << l_name << std::endl;*/
        }

        void start_discovery()
        {
            clear_discovery_complete();

            auto requestedProperties = single_threaded_vector<hstring>({ L"System.Devices.Aep.DeviceAddress", L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.Bluetooth.Le.IsConnectable" });
            // BT_Code: Example showing paired and non-paired in a single query.
            hstring aqsAllBluetoothLEDevices = L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";
            m_deviceWatcher =
                Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
                    aqsAllBluetoothLEDevices,
                    requestedProperties,
                    DeviceInformationKind::AssociationEndpoint);
            // Register event handlers before starting the watcher.
            m_deviceWatcherAddedToken = m_deviceWatcher.Added(&Discovery::DeviceWatcher_Added);
            m_deviceWatcherUpdatedToken = m_deviceWatcher.Updated(&Discovery::DeviceWatcher_Updated);
            m_deviceWatcherRemovedToken = m_deviceWatcher.Removed(&Discovery::DeviceWatcher_Removed);
            m_deviceWatcherEnumerationCompletedToken = m_deviceWatcher.EnumerationCompleted(&Discovery::DeviceWatcher_EnumerationCompleted);
            m_deviceWatcherStoppedToken = m_deviceWatcher.Stopped(&Discovery::DeviceWatcher_Stopped);

            // Start the watcher. Active enumeration is limited to approximately 30 seconds.
            // This limits power usage and reduces interference with other Bluetooth activities.
            // To monitor for the presence of Bluetooth LE devices for an extended period,
            // use the BluetoothLEAdvertisementWatcher runtime class. See the BluetoothAdvertisement
            // sample for an example.
            m_deviceWatcher.Start();

        }

        bool wait_for_discovery_complete()
        {
            if (m_discovery_complete.wait(32000) != 0)
                return false;
            else
                return true;
        }

        void clear_discovery_complete()
        {
            m_discovery_complete.reset();
        }

        void signal_discovery_complete()
        {
            m_discovery_complete.set();
        }

        static Discovery* getDiscovery()
        {
            if (!sm_dc)
            {
                sm_dc = new Discovery();
            }
            return sm_dc;
        }

        jobjectArray getJavaBLEDeviceList(JNIEnv* ap_jenv)
        { 
            std::map<std::wstring, Windows::Devices::Enumeration::DeviceInformation*>::iterator l_end, l_ptr;
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);

            jstring l_str;
            jobjectArray l_ids = 0;
            jsize l_len = (jsize)m_id_to_di.size();

            l_ids = ap_jenv->NewObjectArray(l_len, ap_jenv->FindClass("java/lang/String"), 0);

            int l_count = 0;
            for (l_end = m_id_to_di.end(), l_ptr= m_id_to_di.begin(); l_ptr!=l_end; ++l_ptr)
            {
                std::wstring l_id( (l_ptr->second)->Id().c_str());
                l_str = ap_jenv->NewString((jchar *)l_id.c_str(), (jsize)l_id.length());
                ap_jenv->SetObjectArrayElement(l_ids, l_count++, l_str);
            }

            return l_ids;
        }

        IAsyncOperation<bool> GetBLEDevice(Windows::Devices::Bluetooth::BluetoothLEDevice& ar_bled, hstring &ar_id, concurrency::event& ar_wait)
        {
            Windows::Devices::Bluetooth::BluetoothLEDevice l_bluetoothLeDevice = co_await BluetoothLEDevice::FromIdAsync(ar_id);
            ar_bled = l_bluetoothLeDevice;
            ar_wait.set();
            co_return true;
        }

        IAsyncOperation<bool> GetGattServices(Windows::Devices::Bluetooth::BluetoothLEDevice& ar_bled, concurrency::event& ar_wait)
        {
            std::cout << "Wait for Gatt Services" << std::endl;
            m_temp_gdsr = co_await ar_bled.GetGattServicesAsync(BluetoothCacheMode::Uncached);
            std::cout << "Got Gatt Services" << std::endl;
            ar_wait.set();
            if (m_temp_gdsr.Status() != GattCommunicationStatus::Success) co_return false;
            else co_return true;
        }

        jobjectArray getJavaBLEDeviceServices(JNIEnv* ap_jenv, jstring a_id)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);

            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_di[l_id];
            if (!lp_di) return NULL;

            Windows::Devices::Bluetooth::BluetoothLEDevice l_bluetoothLeDevice{ nullptr };

            concurrency::event l_wait;
            IAsyncOperation<bool> l_ret = GetBLEDevice(l_bluetoothLeDevice, lp_di->Id(), l_wait);
            if (l_wait.wait(20000) != 0) {
                std::cout << "Failed to get BLE dev" << std::endl;
                return NULL;
            }
            l_wait.reset();
            std::cout << "1" << std::endl;
            //if (l_ret.get() != true) return NULL;
            l_ret = GetGattServices(l_bluetoothLeDevice, l_wait);
            if (l_wait.wait(40000) != 0) {
                std::cout << "Failed to get BLE services" << std::endl;
                return NULL;
            }
            std::cout << "2" << std::endl;
            //if (l_ret.get() != true) return NULL;

            jstring l_str;
            jobjectArray l_svcs = 0;
            jsize l_len = (jsize)m_temp_gdsr.Services().Size();

            l_svcs = ap_jenv->NewObjectArray(l_len, ap_jenv->FindClass("java/lang/String"), 0);

            for (unsigned int i=0; i< m_temp_gdsr.Services().Size();++i)
            {
                std::wstringstream l_uuid;
                GattDeviceService& lr_gds = m_temp_gdsr.Services().GetAt(i);
                l_uuid.fill('0');
                l_uuid << std::uppercase << std::hex
                    << std::setw(8)
                    << lr_gds.Uuid().Data1
                    << '-' << std::setw(4) << lr_gds.Uuid().Data2
                    << '-' << std::setw(4) << lr_gds.Uuid().Data3
                    << '-' << std::setw(2) << (unsigned int)(lr_gds.Uuid().Data4[0])
                    << std::setw(2)
                    << (unsigned int)(lr_gds.Uuid().Data4[1])
                    << '-' << std::setw(2) << (unsigned int)(lr_gds.Uuid().Data4[2])
                    << std::setw(2)
                    << (unsigned int)(lr_gds.Uuid().Data4[3])
                    << std::setw(2)
                    << (unsigned int)(lr_gds.Uuid().Data4[4])
                    << std::setw(2)
                    << (unsigned int)(lr_gds.Uuid().Data4[5])
                    << std::setw(2)
                    << (unsigned int)(lr_gds.Uuid().Data4[6])
                    << std::setw(2)
                    << (unsigned int)(lr_gds.Uuid().Data4[7]);
                l_str = ap_jenv->NewString((jchar*)l_uuid.str().c_str(), (jsize)l_uuid.str().length());
                ap_jenv->SetObjectArrayElement(l_svcs, i, l_str);
            }

            return l_svcs;
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
            std::wstring l_id(deviceInfoUpdate.Id().c_str());
            sm_dc->remove_di(l_id);
        };
        static void DeviceWatcher_EnumerationCompleted(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Foundation::IInspectable const&)
        {
            std::wcout << "In " << __func__ << std::endl;
            if (!sm_dc) return;
            sm_dc->signal_discovery_complete();
        };
        static void DeviceWatcher_Stopped(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Foundation::IInspectable const&)
        {
            std::wcout << "In " << __func__ << std::endl;
            if (!sm_dc) return;
            sm_dc->signal_discovery_complete();
        };
};

Discovery *Discovery::sm_dc = NULL;

JNIEXPORT jobjectArray JNICALL Java_javelin_1test_javelin_listBLEDevices
(JNIEnv *ap_jenv, jclass)
{
	std::cout << "Hello from JNI C++" << std::endl;

    Discovery *lp_dc = Discovery::getDiscovery();
    lp_dc->start_discovery();

    if (lp_dc->wait_for_discovery_complete())
    {
        return lp_dc->getJavaBLEDeviceList(ap_jenv);
    }
    else
    {
        return NULL;
    }
}

JNIEXPORT jobjectArray JNICALL Java_javelin_1test_javelin_listBLEDeviceServices
(JNIEnv *ap_jenv, jclass, jstring a_id)
{
    Discovery* lp_dc = Discovery::getDiscovery();
    return lp_dc->getJavaBLEDeviceServices(ap_jenv, a_id);
}

JNIEXPORT jstring JNICALL Java_javelin_1test_javelin_getBLEDeviceName
(JNIEnv * ap_jenv, jclass, jstring a_id)
{
    std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
    std::wstring l_name = Discovery::getDiscovery()->getName(l_id);

    jstring l_str;
    l_str = ap_jenv->NewString((jchar*)l_name.c_str(), (jsize)l_name.length());

    return l_str;
}

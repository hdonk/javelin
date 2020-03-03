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
using namespace Windows::Storage::Streams;
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

void guidTowstring(guid& ar_guid, std::wstring& ar_wstring)
{
    std::wstringstream l_uuid;

    l_uuid.fill('0');
    l_uuid << std::uppercase << std::hex
        << std::setw(8)
        << ar_guid.Data1
        << '-' << std::setw(4) << ar_guid.Data2
        << '-' << std::setw(4) << ar_guid.Data3
        << '-' << std::setw(2) << (unsigned int)(ar_guid.Data4[0])
        << std::setw(2)
        << (unsigned int)(ar_guid.Data4[1])
        << '-' << std::setw(2) << (unsigned int)(ar_guid.Data4[2])
        << std::setw(2)
        << (unsigned int)(ar_guid.Data4[3])
        << std::setw(2)
        << (unsigned int)(ar_guid.Data4[4])
        << std::setw(2)
        << (unsigned int)(ar_guid.Data4[5])
        << std::setw(2)
        << (unsigned int)(ar_guid.Data4[6])
        << std::setw(2)
        << (unsigned int)(ar_guid.Data4[7])
        << std::flush;

    ar_wstring = l_uuid.str();
}

class BLEService
{
    public:
        GattDeviceService* mp_ds;
        std::map < std::wstring, GattCharacteristic*> m_uuid_to_characteristic;

        BLEService()
        {
        }
        ~BLEService()
        {
            {
                std::map < std::wstring, GattCharacteristic *>::iterator l_ptr, l_end;
                for (l_end = m_uuid_to_characteristic.end(), l_ptr = m_uuid_to_characteristic.begin(); l_ptr != l_end; ++l_ptr)
                {
                    delete l_ptr->second;
                }
            }

            delete mp_ds;
        }
};

class BLEDevice
{
    public:
        Windows::Devices::Enumeration::DeviceInformation* mp_di;
        Windows::Devices::Bluetooth::BluetoothLEDevice* mp_bled;

        std::map < std::wstring, BLEService *> m_uuid_to_service;
        BLEDevice()
            : mp_di(0)
            , mp_bled(0)
        {
        }

        ~BLEDevice()
        {
            {
                std::map < std::wstring, BLEService *>::iterator l_ptr, l_end;
                for (l_end = m_uuid_to_service.end(), l_ptr = m_uuid_to_service.begin(); l_ptr != l_end; ++l_ptr)
                {
                    delete l_ptr->second;
                }
            }
            if (mp_bled)
            {
                mp_bled->Close();
                delete mp_bled;
            }
            delete mp_di;
        }
};

class Discovery
{
        static Discovery* sm_dc;

        concurrency::critical_section m_lock_std;
        std::map<std::wstring, BLEDevice *> m_id_to_bd;
        concurrency::event m_discovery_complete;

        Windows::Devices::Enumeration::DeviceWatcher m_deviceWatcher{ nullptr };
        event_token m_deviceWatcherAddedToken;
        event_token m_deviceWatcherUpdatedToken;
        event_token m_deviceWatcherRemovedToken;
        event_token m_deviceWatcherEnumerationCompletedToken;
        event_token m_deviceWatcherStoppedToken;


    public:
        Discovery()
            : m_lock_std()
            , m_id_to_bd()
            , m_discovery_complete()
        {
            m_discovery_complete.reset();
        };

        ~Discovery()
        {
            std::map<std::wstring, BLEDevice*>::iterator l_end, l_ptr;
            
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);

            if (m_deviceWatcher != nullptr)
            {
                // Unregister the event handlers.
                m_deviceWatcher.Added(m_deviceWatcherAddedToken);
                m_deviceWatcher.Updated(m_deviceWatcherUpdatedToken);
                m_deviceWatcher.Removed(m_deviceWatcherRemovedToken);
                m_deviceWatcher.EnumerationCompleted(m_deviceWatcherEnumerationCompletedToken);
                m_deviceWatcher.Stopped(m_deviceWatcherStoppedToken);

                // Stop the watcher.
                m_deviceWatcher.Stop();
                m_deviceWatcher = nullptr;
            }

            for (l_end = m_id_to_bd.end(), l_ptr = m_id_to_bd.begin(); l_ptr != l_end; ++l_ptr)
            {
                delete l_ptr->second;
            }
        }

        void add_di(std::wstring &ar_id, Windows::Devices::Enumeration::DeviceInformation ar_di)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            BLEDevice *lp_bd = ::new BLEDevice();
            lp_bd->mp_di = ::new Windows::Devices::Enumeration::DeviceInformation(ar_di);
            m_id_to_bd[ar_id] = lp_bd;
            display_di(lp_bd->mp_di);
        }

        void update_di(std::wstring& ar_id, Windows::Devices::Enumeration::DeviceInformationUpdate ar_diu)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            m_id_to_bd[ar_id]->mp_di->Update(ar_diu);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[ar_id]->mp_di;
            display_di(lp_di);
        }

        void remove_di(std::wstring& ar_id)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[ar_id]->mp_di;
            display_di(lp_di);
            ::delete m_id_to_bd[ar_id];
            m_id_to_bd.erase(ar_id);
        }

        std::wstring getName(std::wstring& ar_id)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[ar_id]->mp_di;
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
            std::map<std::wstring, BLEDevice *>::iterator l_end, l_ptr;
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);

            jstring l_str;
            jobjectArray l_ids = 0;
            jsize l_len = (jsize)m_id_to_bd.size();

            l_ids = ap_jenv->NewObjectArray(l_len, ap_jenv->FindClass("java/lang/String"), 0);

            int l_count = 0;
            for (l_end = m_id_to_bd.end(), l_ptr= m_id_to_bd.begin(); l_ptr!=l_end; ++l_ptr)
            {
                std::wstring l_id( (l_ptr->second)->mp_di->Id().c_str());
                l_str = ap_jenv->NewString((jchar *)l_id.c_str(), (jsize)l_id.length());
                ap_jenv->SetObjectArrayElement(l_ids, l_count++, l_str);
            }

            return l_ids;
        }

        IAsyncOperation<bool> setValueUWP(GattDeviceService& ar_gds,
            guid& ar_uuid, int32_t a_value)
        {
            DataWriter writer;
            writer.ByteOrder(ByteOrder::LittleEndian);
            writer.WriteInt32(a_value);
            GattDeviceServicesResult l_gdsr{ nullptr };
            //l_gdsr.Services().
            //GattWriteResult result = co_await selectedCharacteristic.WriteValueWithResultAsync(writer.DetachBuffer());

            co_return true;
        }

        IAsyncOperation<bool> GetBLEDeviceUWP(Windows::Devices::Bluetooth::BluetoothLEDevice& ar_bled, hstring &ar_id)
        {
            Windows::Devices::Bluetooth::BluetoothLEDevice l_bluetoothLeDevice = co_await BluetoothLEDevice::FromIdAsync(ar_id);
            ar_bled = l_bluetoothLeDevice;
            co_return true;
        }

        bool GetBLEDevice(BLEDevice* ap_bd)
        {
            Windows::Devices::Enumeration::DeviceInformation* lp_di = ap_bd->mp_di;
            if (!lp_di) return false;
            if (ap_bd->mp_bled) return true;

            Windows::Devices::Bluetooth::BluetoothLEDevice l_bluetoothLeDevice{ nullptr };
            IAsyncOperation<bool> l_ret = GetBLEDeviceUWP(l_bluetoothLeDevice, lp_di->Id());
            if (!l_ret.get()) {
                std::cout << "Failed to get BLE dev" << std::endl;
                return false;
            }

            ap_bd->mp_bled = ::new Windows::Devices::Bluetooth::BluetoothLEDevice(l_bluetoothLeDevice);

            return true;
        }

        IAsyncOperation<bool> GetGattServicesUWP(Windows::Devices::Bluetooth::BluetoothLEDevice *ap_bled,
            GattDeviceServicesResult &ar_gdsr)
        {
            ar_gdsr = co_await ap_bled->GetGattServicesAsync(BluetoothCacheMode::Uncached);
            if (ar_gdsr.Status() != GattCommunicationStatus::Success) co_return false;
            else co_return true;
        }

        bool GetGattServices(BLEDevice* ap_bd, GattDeviceServicesResult& ar_gdsr)
        {
            IAsyncOperation<bool> l_ret = GetGattServicesUWP(ap_bd->mp_bled, ar_gdsr);

            if (!l_ret.get())
            {
                std::cout << "Failed to get gatt svcs" << std::endl;
                return false;
            }
            else
                return true;
        }

        jobjectArray getJavaBLEDeviceServices(JNIEnv* ap_jenv, jstring a_id)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);

            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di) return NULL;

            Windows::Devices::Bluetooth::BluetoothLEDevice l_bluetoothLeDevice{ nullptr };

            bool l_ret = GetBLEDevice(m_id_to_bd[l_id]);
            if (l_ret != true) return NULL;

            GattDeviceServicesResult l_gdsr{ nullptr };
            l_ret = GetGattServices(m_id_to_bd[l_id], l_gdsr);
            if (l_ret != true) {
                std::cout << "Failed to get BLE services" << std::endl;
                return NULL;
            }

            jstring l_str;
            jobjectArray l_svcs = 0;
            jsize l_len = (jsize)l_gdsr.Services().Size();

            l_svcs = ap_jenv->NewObjectArray(l_len, ap_jenv->FindClass("java/lang/String"), 0);

            for (unsigned int i=0; i< l_gdsr.Services().Size();++i)
            {
                std::wstring l_uuid;
                guidTowstring(l_gdsr.Services().GetAt(i).Uuid(), l_uuid);
                m_id_to_bd[l_id]->m_uuid_to_service[l_uuid]->mp_ds = ::new GattDeviceService(l_gdsr.Services().GetAt(i));
                l_str = ap_jenv->NewString((jchar*)l_uuid.c_str(), (jsize)l_uuid.length());
                ap_jenv->SetObjectArrayElement(l_svcs, i, l_str);
            }
            m_id_to_bd[l_id]->mp_bled->Close();
            delete m_id_to_bd[l_id]->mp_bled;
            m_id_to_bd[l_id]->mp_bled = NULL;

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

BOOL APIENTRY DllMain(HMODULE /* hModule */, DWORD ul_reason_for_call, LPVOID /* lpReserved */)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        delete  Discovery::getDiscovery();
        break;
    }
    return TRUE;
}

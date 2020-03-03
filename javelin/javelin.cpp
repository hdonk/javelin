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
#define MARK std::cout << __func__ << ':' << __LINE__ << std::endl;
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
            : mp_ds(0)
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
            if (m_id_to_bd.find(ar_id) == m_id_to_bd.end()) return std::wstring();
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

        IAsyncOperation<bool> GetBLEDeviceUWP(Windows::Devices::Bluetooth::BluetoothLEDevice& ar_bled, hstring &ar_id)
        {
            Windows::Devices::Bluetooth::BluetoothLEDevice l_bld = co_await BluetoothLEDevice::FromIdAsync(ar_id);
            ar_bled = l_bld;
            co_return true;
        }

        bool GetBLEDevice(BLEDevice* ap_bd)
        {
            Windows::Devices::Enumeration::DeviceInformation* lp_di = ap_bd->mp_di;
            if (!lp_di) return false;
            if (ap_bd->mp_bled) return true;

            Windows::Devices::Bluetooth::BluetoothLEDevice l_bld{ nullptr };
            IAsyncOperation<bool> l_ret = GetBLEDeviceUWP(l_bld, lp_di->Id());
            if (!l_ret.get()) {
                std::cout << "Failed to get BLE dev" << std::endl;
                return false;
            }

            ap_bd->mp_bled = ::new Windows::Devices::Bluetooth::BluetoothLEDevice(l_bld);

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
            if (m_id_to_bd.find(l_id) == m_id_to_bd.end()) return NULL;
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di) return NULL;
            Windows::Devices::Bluetooth::BluetoothLEDevice l_bld{ nullptr };

            if (m_id_to_bd[l_id]->mp_bled)
            {
                m_id_to_bd[l_id]->mp_bled->Close();
                delete m_id_to_bd[l_id]->mp_bled;
                m_id_to_bd[l_id]->mp_bled = NULL;
            }
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

            std::cout << "Found " << l_len << " services." << std::endl;
            l_svcs = ap_jenv->NewObjectArray(l_len, ap_jenv->FindClass("java/lang/String"), 0);
            for (unsigned int i=0; i< l_gdsr.Services().Size();++i)
            {
                std::wstring l_uuid;
                guidTowstring(l_gdsr.Services().GetAt(i).Uuid(), l_uuid);
                if (!m_id_to_bd[l_id]->m_uuid_to_service[l_uuid])
                {
                    m_id_to_bd[l_id]->m_uuid_to_service[l_uuid] = new BLEService();
                }
                m_id_to_bd[l_id]->m_uuid_to_service[l_uuid]->mp_ds = ::new GattDeviceService(l_gdsr.Services().GetAt(i));
                l_str = ap_jenv->NewString((jchar*)l_uuid.c_str(), (jsize)l_uuid.length());
                ap_jenv->SetObjectArrayElement(l_svcs, i, l_str);
            }
            return l_svcs;
        }

        IAsyncOperation<bool> GetGattCharacteristicsUWP(GattDeviceService *ap_gds,
            GattCharacteristicsResult &ar_gcr)
        {
            ar_gcr = co_await ap_gds->GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
            if (ar_gcr.Status() != GattCommunicationStatus::Success) co_return false;
            else co_return true;
        }

        bool GetGattCharacteristics(BLEService *ap_gds, GattCharacteristicsResult &ar_gcr, std::wstring &ar_service)
        {
            IAsyncOperation<bool> l_ret = GetGattCharacteristicsUWP(ap_gds->mp_ds, ar_gcr);

            if (!l_ret.get())
            {
                std::cout << "Failed to get gatt svcs characteristics" << std::endl;
                return false;
            }
            else
                return true;
        }

        jobjectArray getJavaBLEServiceCharacteristics(JNIEnv* ap_jenv, jstring a_id, jstring a_service)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);

            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            std::wstring l_service = Java_To_WStr(ap_jenv, a_service);

            if (m_id_to_bd.find(l_id) == m_id_to_bd.end()) return NULL;

            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di) return NULL;
            if (m_id_to_bd[l_id]->m_uuid_to_service.find(l_service) == m_id_to_bd[l_id]->m_uuid_to_service.end()) return NULL;
            
            GattCharacteristicsResult l_gcr{ nullptr };
            bool l_ret = GetGattCharacteristics(m_id_to_bd[l_id]->m_uuid_to_service[l_service], l_gcr, l_service);
            if (l_ret != true) {
                std::cout << "Failed to get BLE service characteristics" << std::endl;
                return NULL;
            }

            jstring l_str;
            jobjectArray l_characteristics = 0;
            jsize l_len = (jsize)l_gcr.Characteristics().Size();

            l_characteristics = ap_jenv->NewObjectArray(l_len, ap_jenv->FindClass("java/lang/String"), 0);

            for (unsigned int i = 0; i < l_gcr.Characteristics().Size(); ++i)
            {
                std::wstring l_uuid;
                guidTowstring(l_gcr.Characteristics().GetAt(i).Uuid(), l_uuid);
                std::wcout << "Char " << l_uuid << " added " << std::endl;
                m_id_to_bd[l_id]->m_uuid_to_service[l_service]->m_uuid_to_characteristic[l_uuid] =
                        ::new GattCharacteristic(l_gcr.Characteristics().GetAt(i));
                l_str = ap_jenv->NewString((jchar*)l_uuid.c_str(), (jsize)l_uuid.length());
                ap_jenv->SetObjectArrayElement(l_characteristics, i, l_str);
            }
            return l_characteristics;
        }

        std::string to_string(GattCommunicationStatus status)
        {
            switch (status)
            {
            case GattCommunicationStatus::Success: return std::string("Success");
            case GattCommunicationStatus::Unreachable: return std::string("Unreachable");
            case GattCommunicationStatus::ProtocolError: return std::string("ProtocolError");
            case GattCommunicationStatus::AccessDenied: return std::string("AccessDenied");
            default: return std::string("Unknown");
            }
        }

        IAsyncOperation<bool> GetGattCharacteristicValueUWP(GattCharacteristic* ap_gc,
            GattReadResult& ar_grr, DataReader &ar_dr)
        {
            ar_grr = co_await ap_gc->ReadValueAsync(BluetoothCacheMode::Uncached);
            MARK;
            if (ar_grr.Status() != GattCommunicationStatus::Success)
            {
                std::cerr << "Failed to read Gatt Characteristic "
                    << " Due to " << to_string(ar_grr.Status()) << std::endl;
                co_return false;
            }
            else
            {
                ar_dr = DataReader::FromBuffer(ar_grr.Value());
                std::cout << "OK!" << std::endl;
                co_return true;
            }
        }

        bool GetGattCharacteristicValue(GattCharacteristic* ap_gc, GattReadResult& ar_grr, DataReader &ar_dr)
        {
            std::wstring l_uuid;
            guidTowstring(ap_gc->Uuid(), l_uuid);
            std::wcout << __func__ << " Char " << l_uuid << std::endl;

            IAsyncOperation<bool> l_ret = GetGattCharacteristicValueUWP(ap_gc, ar_grr, ar_dr);
            MARK;
            if (!l_ret.get())
            {
                std::cout << "Failed to get gatt svcs characteristic value" << std::endl;
                return false;
            }
            else
                return true;
        }

        bool getJavaBLECharacteristicValue(JNIEnv* ap_jenv, jstring a_id, jstring a_service, jstring a_characteristic, DataReader &ar_dr)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            MARK;
            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            std::wstring l_service = Java_To_WStr(ap_jenv, a_service);
            std::wstring l_char = Java_To_WStr(ap_jenv, a_characteristic);
            std::wcout << "lf " << l_service << '/' << l_char << std::endl;

            if (m_id_to_bd.find(l_id) == m_id_to_bd.end()) return false;
            MARK;
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di) return false;
            MARK;
            if (m_id_to_bd[l_id]->m_uuid_to_service.find(l_service) == m_id_to_bd[l_id]->m_uuid_to_service.end()) return false;
            BLEService* lp_bs = m_id_to_bd[l_id]->m_uuid_to_service[l_service];
            if (!lp_bs) return false;
            MARK;
            if (lp_bs->m_uuid_to_characteristic.find(l_char) == lp_bs->m_uuid_to_characteristic.end()) return false;
            GattCharacteristic* lp_gc = lp_bs->m_uuid_to_characteristic[l_char];
            if (!lp_gc) return false;
            MARK;
            GattReadResult l_grr{ nullptr };
            bool l_ret = GetGattCharacteristicValue(lp_gc, l_grr, ar_dr);
            return l_ret;
        }

        IAsyncOperation<bool> SetGattCharacteristicValueUWP(GattCharacteristic* ap_gc,
            GattWriteResult& ar_gwr, DataWriter &ar_dw)
        {
            MARK;
            ar_gwr = co_await ap_gc->WriteValueWithResultAsync(ar_dw.DetachBuffer());
            MARK;
            if (ar_gwr.Status() != GattCommunicationStatus::Success)
            {
                std::cerr << "Failed to write Gatt Characteristic due to " << 
                        to_string(ar_gwr.Status()) << std::endl;
                co_return false;
            }
            else
            {
                std::cout << "OK!" << std::endl;
                co_return true;
            }
        }

        bool SetGattCharacteristicValue(GattCharacteristic* ap_gc, GattWriteResult& ar_gwr, DataWriter &ar_dw)
        {
            IAsyncOperation<bool> l_ret = SetGattCharacteristicValueUWP(ap_gc, ar_gwr, ar_dw);
            MARK;
            if (!l_ret.get())
            {
                std::cout << "Failed to set gatt svcs characteristic value" << std::endl;
                return false;
            }
            else
                return true;
        }

        bool setJavaBLECharacteristicValue(JNIEnv* ap_jenv, jstring a_id, jstring a_service, jstring a_characteristic, DataWriter &ar_dw)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            MARK;
            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            std::wstring l_service = Java_To_WStr(ap_jenv, a_service);
            std::wstring l_char = Java_To_WStr(ap_jenv, a_characteristic);

            if (m_id_to_bd.find(l_id) == m_id_to_bd.end()) return false;
            MARK;
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di) return false;
            MARK;
            if (m_id_to_bd[l_id]->m_uuid_to_service.find(l_service) == m_id_to_bd[l_id]->m_uuid_to_service.end()) return false;
            BLEService* lp_bs = m_id_to_bd[l_id]->m_uuid_to_service[l_service];
            if (!lp_bs) return false;
            MARK;
            if (lp_bs->m_uuid_to_characteristic.find(l_char) == lp_bs->m_uuid_to_characteristic.end()) return false;
            GattCharacteristic* lp_gc = lp_bs->m_uuid_to_characteristic[l_char];
            if (!lp_gc) return false;
            MARK;
            GattWriteResult l_gwr{ nullptr };
            return SetGattCharacteristicValue(lp_gc, l_gwr, ar_dw);
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
(JNIEnv *ap_jenv, jclass, jstring a_id)
{
    std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
    std::wstring l_name = Discovery::getDiscovery()->getName(l_id);

    jstring l_str;
    l_str = ap_jenv->NewString((jchar*)l_name.c_str(), (jsize)l_name.length());

    return l_str;
}

JNIEXPORT jobjectArray JNICALL Java_javelin_1test_javelin_listBLEServiceCharacteristics
(JNIEnv *ap_jenv, jclass, jstring a_id, jstring a_service)

{
    Discovery* lp_dc = Discovery::getDiscovery();
    return lp_dc->getJavaBLEServiceCharacteristics(ap_jenv, a_id, a_service);
}

JNIEXPORT jbyteArray JNICALL Java_javelin_1test_javelin_getBLECharacteristicValue
(JNIEnv* ap_jenv, jclass, jstring a_id, jstring a_service, jstring a_characteristic)
{
    Discovery* lp_dc = Discovery::getDiscovery();
    DataReader l_dr{ nullptr };
    if(lp_dc->getJavaBLECharacteristicValue(ap_jenv, a_id, a_service, a_characteristic, l_dr))
    {
        std::vector<byte> l_bytes;
        int l_len = l_dr.UnconsumedBufferLength();
        jbyte *lp_jbytes = ::new jbyte[l_len];
        jbyteArray l_ba = ap_jenv->NewByteArray(l_len);
        for(int i=0; i<l_len; ++i)
            lp_jbytes[i] = l_dr.ReadByte();
        ap_jenv->SetByteArrayRegion(l_ba, 0, l_len, lp_jbytes);
        delete lp_jbytes;
        return l_ba;
    }
    else
        return NULL;
}

JNIEXPORT jboolean JNICALL Java_javelin_1test_javelin_setBLECharacteristicValue
(JNIEnv * ap_jenv, jclass, jstring a_id, jstring a_service, jstring a_characteristic, jbyteArray a_value)
{
    Discovery* lp_dc = Discovery::getDiscovery();
    jbyte *lp_jbytes;
    lp_jbytes = ap_jenv->GetByteArrayElements(a_value, 0);
    DataWriter l_dw;
    l_dw.ByteOrder(ByteOrder::LittleEndian);
    for(int i=0; i<ap_jenv->GetArrayLength(a_value); ++i)
        l_dw.WriteByte(lp_jbytes[i]);
    ap_jenv->ReleaseByteArrayElements(a_value, lp_jbytes, 0);
    return lp_dc->setJavaBLECharacteristicValue(ap_jenv, a_id, a_service, a_characteristic, l_dw);
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

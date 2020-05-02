#include "pch.h"
#include "javelin.h"

#include <../javelin_test/bin/javelin_javelin.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <concrt.h>
#include <queue>

using namespace winrt;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;
//using namespace Windows::Foundation::Collections::IVectorView;

// Utility Functions
#if defined(_DEBUG)
#define MARK std::wcerr << __func__ << ':' << __LINE__ << std::endl;
#define DEBUG_TRACE(reason) std::wcerr << __func__ << ':' << __LINE__ << ' ' << reason << std::endl;
#else
#define MARK
#define DEBUG_TRACE(reason)
#endif

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

std::wstring gcs_to_wstring(GattCommunicationStatus status)
{
    switch (status)
    {
    case GattCommunicationStatus::Success: return std::wstring(L"Success");
    case GattCommunicationStatus::Unreachable: return std::wstring(L"Unreachable");
    case GattCommunicationStatus::ProtocolError: return std::wstring(L"ProtocolError");
    case GattCommunicationStatus::AccessDenied: return std::wstring(L"AccessDenied");
    default: return std::wstring(L"Unknown");
    }
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
                    MARK;
                    delete l_ptr->second;
                }
            }
            MARK;
            mp_ds->Session().MaintainConnection(false);
            MARK;
            //delete mp_ds;
            MARK;
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
                MARK;
                std::map < std::wstring, BLEService *>::iterator l_ptr, l_end;
                for (l_end = m_uuid_to_service.end(), l_ptr = m_uuid_to_service.begin(); l_ptr != l_end; ++l_ptr)
                {
                    MARK;
                    delete l_ptr->second;
                }
            }
            MARK;
            if (mp_bled)
            {
                MARK;
  //              mp_bled->Close();
                MARK;
                delete mp_bled;
            }
            MARK;
            delete mp_di;
        }
};

class Discovery
{
        static Discovery* smp_dc;

        concurrency::critical_section m_lock_std;
        std::map<std::wstring, BLEDevice *> m_id_to_bd;
        concurrency::event m_discovery_complete;

        Windows::Devices::Enumeration::DeviceWatcher m_deviceWatcher{ nullptr };
        event_token m_deviceWatcherAddedToken;
        event_token m_deviceWatcherUpdatedToken;
        event_token m_deviceWatcherRemovedToken;
        event_token m_deviceWatcherEnumerationCompletedToken;
        event_token m_deviceWatcherStoppedToken;

        std::map < std::wstring, event_token> m_uuid_to_notification_token;
        std::map < std::wstring, concurrency::event> m_uuid_to_notification_event;
        std::map < std::wstring, GattCharacteristic*> m_uuid_to_notification_characteristic;
        std::map < std::wstring, std::queue<std::string *> > m_uuid_to_changed_value;


    public:
        Discovery()
            : m_lock_std()
            , m_id_to_bd()
            , m_discovery_complete()
        {
            std::cout << "javelin starting" << std::endl;

            m_discovery_complete.reset();
        };

        ~Discovery()
        {
            MARK;
            //concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            MARK;
            if (m_deviceWatcher != nullptr)
            {
                MARK;
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
            std::cout << "javelin finished" << std::endl;
            return;
#if 0
            {
                MARK;
                std::map < std::wstring, event_token>::iterator l_ptr, l_end;
                for (l_end = m_uuid_to_notification_token.end(), l_ptr = m_uuid_to_notification_token.begin(); l_ptr != l_end; ++l_ptr)
                {
                    GattCharacteristic* lp_gc = m_uuid_to_notification_characteristic[l_ptr->first];
                    if (lp_gc)
                    {
                        lp_gc->ValueChanged(l_ptr->second);
                    }
                }
            }

            {
                MARK;
                std::map<std::wstring, BLEDevice*>::iterator l_end, l_ptr;
                for (l_end = m_id_to_bd.end(), l_ptr = m_id_to_bd.begin(); l_ptr != l_end; ++l_ptr)
                {
                    delete l_ptr->second;
                }
            }

            {
                MARK;
                std::map < std::wstring, std::queue<std::string *> >::iterator l_ptr, l_end;
                for (l_end = m_uuid_to_changed_value.end(), l_ptr = m_uuid_to_changed_value.begin(); l_ptr != l_end; ++l_ptr)
                {
                    while (!l_ptr->second.empty())
                    {
                        std::string *lp_bytes = l_ptr->second.front();
                        l_ptr->second.pop();
                        delete lp_bytes;
                    }
                }
            }

            std::cout << "javelin finished" << std::endl;
#endif
        }

        static void releaseDiscovery()
        {
            MARK;
            if (smp_dc)
            {
                MARK;
                delete smp_dc;
            }
        }
        
        void add_di(std::wstring &ar_id, Windows::Devices::Enumeration::DeviceInformation ar_di)
        {
            MARK;
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            BLEDevice *lp_bd = ::new BLEDevice();
            lp_bd->mp_di = ::new Windows::Devices::Enumeration::DeviceInformation(ar_di);
            m_id_to_bd[ar_id] = lp_bd;
            DEBUG_TRACE("-- Adding");
            display_di(lp_bd->mp_di);
        }

        void update_di(std::wstring& ar_id, Windows::Devices::Enumeration::DeviceInformationUpdate ar_diu)
        {
            MARK;
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            m_id_to_bd[ar_id]->mp_di->Update(ar_diu);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[ar_id]->mp_di;
            DEBUG_TRACE("-- Updating");
            display_di(lp_di);
        }

        void remove_di(std::wstring& ar_id)
        {
            MARK;
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[ar_id]->mp_di;
//            DEBUG_TRACE("-- Deleting");
//            display_di(lp_di);
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
            std::wstring l_id(ap_di->Id().c_str());
            std::wstring l_name(ap_di->Name().c_str());
            DEBUG_TRACE("Id: " << l_id << " Name: " << l_name)
        }

        void start_discovery(JNIEnv* ap_jenv)
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
            if (!smp_dc)
            {
                smp_dc = new Discovery();
            }
            return smp_dc;
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
                std::wcerr << "Failed to get BLE dev" << std::endl;
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
                std::wcerr << "Failed to get gatt svcs" << std::endl;
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
                std::wcerr << "Failed to get BLE services" << std::endl;
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
                if (!m_id_to_bd[l_id]->m_uuid_to_service[l_uuid])
                {
                    m_id_to_bd[l_id]->m_uuid_to_service[l_uuid] = new BLEService();
                }
                GattDeviceService* lp_gds = ::new GattDeviceService(l_gdsr.Services().GetAt(i));
                m_id_to_bd[l_id]->m_uuid_to_service[l_uuid]->mp_ds = lp_gds;
                if (lp_gds->Session().CanMaintainConnection())
                {
                    lp_gds->Session().MaintainConnection(true);
                }
                else
                {
                    DEBUG_TRACE("Can't maintain connection on " << l_id << '/' << l_uuid);
                }
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
                std::wcerr << "Failed to get gatt svcs characteristics" << std::endl;
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
                std::wcerr << "Failed to get BLE service characteristics" << std::endl;
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
                m_id_to_bd[l_id]->m_uuid_to_service[l_service]->m_uuid_to_characteristic[l_uuid] =
                        ::new GattCharacteristic(l_gcr.Characteristics().GetAt(i));
                l_str = ap_jenv->NewString((jchar*)l_uuid.c_str(), (jsize)l_uuid.length());
                ap_jenv->SetObjectArrayElement(l_characteristics, i, l_str);
            }
            return l_characteristics;
        }

        IAsyncOperation<bool> GetGattCharacteristicValueUWP(GattCharacteristic* ap_gc,
            GattReadResult& ar_grr, DataReader &ar_dr)
        {
            ar_grr = co_await ap_gc->ReadValueAsync(BluetoothCacheMode::Uncached);
            if (ar_grr.Status() != GattCommunicationStatus::Success)
            {
                std::wcerr << "Failed to read Gatt Characteristic d Due to " << gcs_to_wstring(ar_grr.Status()) << std::endl;
                co_return false;
            }
            else
            {
                ar_dr = DataReader::FromBuffer(ar_grr.Value());
                co_return true;
            }
        }

        bool GetGattCharacteristicValue(GattCharacteristic* ap_gc, GattReadResult& ar_grr, DataReader &ar_dr)
        {
            std::wstring l_uuid;
            guidTowstring(ap_gc->Uuid(), l_uuid);

            IAsyncOperation<bool> l_ret = GetGattCharacteristicValueUWP(ap_gc, ar_grr, ar_dr);
            if (!l_ret.get())
            {
                std::wcerr << "Failed to get gatt svcs characteristic value" << std::endl;
                return false;
            }
            else
                return true;
        }

        bool getJavaBLECharacteristicValue(JNIEnv* ap_jenv, jstring a_id, jstring a_service, jstring a_characteristic, DataReader &ar_dr)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            std::wstring l_service = Java_To_WStr(ap_jenv, a_service);
            std::wstring l_char = Java_To_WStr(ap_jenv, a_characteristic);

            if (m_id_to_bd.find(l_id) == m_id_to_bd.end()) return false;
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di) return false;
            if (m_id_to_bd[l_id]->m_uuid_to_service.find(l_service) == m_id_to_bd[l_id]->m_uuid_to_service.end()) return false;
            BLEService* lp_bs = m_id_to_bd[l_id]->m_uuid_to_service[l_service];
            if (!lp_bs) return false;
            if (lp_bs->m_uuid_to_characteristic.find(l_char) == lp_bs->m_uuid_to_characteristic.end()) return false;
            GattCharacteristic* lp_gc = lp_bs->m_uuid_to_characteristic[l_char];
            if (!lp_gc) return false;
            GattReadResult l_grr{ nullptr };
            bool l_ret = GetGattCharacteristicValue(lp_gc, l_grr, ar_dr);
            return l_ret;
        }

        IAsyncOperation<bool> SetGattCharacteristicValueUWP(GattCharacteristic* ap_gc,
            GattWriteResult& ar_gwr, DataWriter &ar_dw)
        {
            ar_gwr = co_await ap_gc->WriteValueWithResultAsync(ar_dw.DetachBuffer());
            MARK;
            if (ar_gwr.Status() != GattCommunicationStatus::Success)
            {
                std::wcerr << "Failed to write Gatt Characteristic due to " <<
                        gcs_to_wstring(ar_gwr.Status()) << std::endl;
                MARK;
                co_return false;
            }
            else
            {
                MARK;
                co_return true;
            }
        }

        bool SetGattCharacteristicValue(GattCharacteristic* ap_gc, GattWriteResult& ar_gwr, DataWriter &ar_dw)
        {
            IAsyncOperation<bool> l_ret = SetGattCharacteristicValueUWP(ap_gc, ar_gwr, ar_dw);
            if (!l_ret.get())
            {
                std::wcerr << "Failed to set gatt svcs characteristic value" << std::endl;
                return false;
            }
            else
                return true;
        }

        bool setJavaBLECharacteristicValue(JNIEnv* ap_jenv, jstring a_id, jstring a_service, jstring a_characteristic, DataWriter &ar_dw)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            std::wstring l_service = Java_To_WStr(ap_jenv, a_service);
            std::wstring l_char = Java_To_WStr(ap_jenv, a_characteristic);

            if (m_id_to_bd.find(l_id) == m_id_to_bd.end()) return false;
            MARK;
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di) return false;
            MARK;
            if (m_id_to_bd[l_id]->m_uuid_to_service.find(l_service) == m_id_to_bd[l_id]->m_uuid_to_service.end()) return false;
            MARK;
            BLEService* lp_bs = m_id_to_bd[l_id]->m_uuid_to_service[l_service];
            if (!lp_bs) return false;
            MARK;
            if (lp_bs->m_uuid_to_characteristic.find(l_char) == lp_bs->m_uuid_to_characteristic.end()) return false;
            MARK;
            GattCharacteristic* lp_gc = lp_bs->m_uuid_to_characteristic[l_char];
            if (!lp_gc) return false;
            MARK;
            GattWriteResult l_gwr{ nullptr };
            return SetGattCharacteristicValue(lp_gc, l_gwr, ar_dw);
        }

        static void DeviceWatcher_Added(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Devices::Enumeration::DeviceInformation deviceInfo)
        {
            MARK;
            if (!smp_dc) return;
            std::wstring l_id(deviceInfo.Id().c_str());
            smp_dc->add_di(l_id, deviceInfo);
        };
        static void DeviceWatcher_Updated(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Devices::Enumeration::DeviceInformationUpdate deviceInfoUpdate)
        {
            MARK;
            if (!smp_dc) return;
            std::wstring l_id(deviceInfoUpdate.Id().c_str());
            smp_dc->update_di(l_id, deviceInfoUpdate);
        };
        static void DeviceWatcher_Removed(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Devices::Enumeration::DeviceInformationUpdate deviceInfoUpdate)
        {
            MARK;
            if (!smp_dc) return;
            std::wstring l_id(deviceInfoUpdate.Id().c_str());
            smp_dc->remove_di(l_id);
        };
        static void DeviceWatcher_EnumerationCompleted(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Foundation::IInspectable const&)
        {
            MARK;
            if (!smp_dc) return;
            smp_dc->signal_discovery_complete();
        };
        static void DeviceWatcher_Stopped(Windows::Devices::Enumeration::DeviceWatcher sender, Windows::Foundation::IInspectable const&)
        {
            MARK;
            if (!smp_dc) return;
            smp_dc->signal_discovery_complete();
        };

        static void Characteristic_ValueChanged(GattCharacteristic const &ar_gc, GattValueChangedEventArgs a_args)
        {
            MARK;
            if (!smp_dc) return;
            std::wstring l_gc_uuid;
            guidTowstring(ar_gc.Uuid(), l_gc_uuid);
            DEBUG_TRACE("Got value changed on " << l_gc_uuid);
            DataReader l_dr = DataReader::FromBuffer(a_args.CharacteristicValue());
            MARK;
            smp_dc->signal_characteristic_valuechanged(l_gc_uuid, l_dr);
            DEBUG_TRACE("Handled value changed on " << l_gc_uuid);
        }

        void signal_characteristic_valuechanged(std::wstring& ar_gc_uuid, DataReader &ar_dr)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            MARK;
            if (m_uuid_to_notification_token.find(ar_gc_uuid) == m_uuid_to_notification_token.end())
            {
                std::wcerr << "Notification token not found" << std::endl;
                return;
            }
            MARK;
            if (m_uuid_to_notification_event.find(ar_gc_uuid) == m_uuid_to_notification_event.end())
            {
                std::wcerr << "Notification event not found" << std::endl;
                return;
            }
            MARK;
            std::string *lp_bytes = new std::string();
            int l_len = ar_dr.UnconsumedBufferLength();
            for (int i = 0; i < l_len; ++i)
            {
                (*lp_bytes)+=ar_dr.ReadByte();
//                if (i == 0) std::wcerr << "Got byte 0 -> " << (int)lp_bytes->at(0) << std::endl;
            }
            /*
            MARK;
            jbyte* lp_jbytes = ::new jbyte[l_len];
            DEBUG_TRACE("Created jbyte array size " << l_len);
            MARK;
            DEBUG_TRACE("Created jbyteArray on " << mp_jenv);
            jbyteArray l_jba = mp_jenv->NewByteArray(l_len);
            MARK;
            for (int i = 0; i < l_len; ++i)
            {
                MARK;
                lp_jbytes[i] = ar_dr.ReadByte();
                MARK;
            }
            MARK;
            mp_jenv->SetByteArrayRegion(l_jba, 0, l_len, lp_jbytes);
            delete lp_jbytes;
            MARK;
            */
            m_uuid_to_changed_value[ar_gc_uuid].push(lp_bytes);
            MARK;
            m_uuid_to_notification_event[ar_gc_uuid].set();
            MARK;
        }

        IAsyncOperation<bool> WriteClientCharacteristicConfigurationDescriptorAsync(GattCharacteristic* ap_gc,
            GattClientCharacteristicConfigurationDescriptorValue a_cccdValue)
        {
            GattCommunicationStatus l_gcs = co_await ap_gc->WriteClientCharacteristicConfigurationDescriptorAsync(a_cccdValue);
            if (l_gcs != GattCommunicationStatus::Success)
            {
                std::wcerr << "Failed to write cccd due to " <<
                    gcs_to_wstring(l_gcs) << std::endl;
                co_return false;
            }
            else
            {
                co_return true;
            }
        }

        bool watchJavaBLECharacteristicChanges(JNIEnv* ap_jenv, jstring a_id, jstring a_service, jstring a_characteristic)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            std::wstring l_service = Java_To_WStr(ap_jenv, a_service);
            std::wstring l_char = Java_To_WStr(ap_jenv, a_characteristic);

            if (m_id_to_bd.find(l_id) == m_id_to_bd.end())
            {
                DEBUG_TRACE("Failed to find device " << l_id);
                return false;
            }
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di)
            {
                DEBUG_TRACE("No device information for " << l_id);
                return false;
            }
            if (m_id_to_bd[l_id]->m_uuid_to_service.find(l_service) == m_id_to_bd[l_id]->m_uuid_to_service.end())
            {
                DEBUG_TRACE("No service information for " << l_id << '/' << l_service);
                return false;
            }
            BLEService* lp_bs = m_id_to_bd[l_id]->m_uuid_to_service[l_service];
            if (!lp_bs)
            {
                DEBUG_TRACE("No BLE service for " << l_id << '/' << l_service);
                return false;
            }
            if (lp_bs->m_uuid_to_characteristic.find(l_char) == lp_bs->m_uuid_to_characteristic.end())
            {
                DEBUG_TRACE("No characteristic for " << l_id << '/' << l_service << '/' << l_char);
                return false;
            }
            GattCharacteristic* lp_gc = lp_bs->m_uuid_to_characteristic[l_char];
            if (!lp_gc)
            {
                DEBUG_TRACE("No Gatt characteristic for " << l_id << '/' << l_service << '/' << l_char);
                return false;
            }

            // If we're already registered, don't do it again!
            if (m_uuid_to_notification_token.find(l_char) != m_uuid_to_notification_token.end())
            {
                DEBUG_TRACE("Already registered for changes on " << l_id << '/' << l_service << '/' << l_char);
                return true;
            }
            if (!WriteClientCharacteristicConfigurationDescriptorAsync(lp_gc, GattClientCharacteristicConfigurationDescriptorValue::Notify).get())
            {
                DEBUG_TRACE("Failed to write ccd for " << l_id << '/' << l_service << '/' << l_char);
                return false;
            }
            DEBUG_TRACE("Registered " << l_char);
            m_uuid_to_notification_characteristic[l_char] = lp_gc;
            m_uuid_to_notification_token[l_char] = lp_gc->ValueChanged(&Discovery::Characteristic_ValueChanged);
            return true;
        }

        bool clearJavaBLECharacteristicChanges(JNIEnv* ap_jenv, jstring a_id, jstring a_service, jstring a_characteristic)
        {
            std::wstring l_char = Java_To_WStr(ap_jenv, a_characteristic);
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            m_uuid_to_notification_event[l_char].reset();
            return true;
        }

        jbyteArray waitForJavaBLECharacteristicChanges(JNIEnv* ap_jenv, jstring a_id, jstring a_service, jstring a_characteristic, jint a_timeout_ms)
        {
            std::wstring l_char = Java_To_WStr(ap_jenv, a_characteristic);
            if (m_uuid_to_notification_event[l_char].wait(a_timeout_ms) != 0) return NULL;
            m_uuid_to_notification_event[l_char].reset();

            std::string *lp_string = m_uuid_to_changed_value[l_char].front();
            m_uuid_to_changed_value[l_char].pop();

            MARK;
            int l_len = (int)lp_string->size();
            jbyte* lp_jbytes = ::new jbyte[lp_string->size()];
            DEBUG_TRACE("Created jbyte array size " << l_len << std::endl);
            MARK;
            DEBUG_TRACE("Create jbyteArray on " << ap_jenv << std::endl);
            jbyteArray l_jba = ap_jenv->NewByteArray(l_len);
            MARK;
            for (int i = 0; i < l_len; ++i)
            {
                MARK;
                lp_jbytes[i] = (*lp_string)[i];
                MARK;
            }
            MARK;
            ap_jenv->SetByteArrayRegion(l_jba, 0, l_len, lp_jbytes);
            delete lp_jbytes;
            MARK;
            return l_jba;
        }

        bool unWatchJavaBLECharacteristicChanges(JNIEnv* ap_jenv, jstring a_id, jstring a_service, jstring a_characteristic)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
            std::wstring l_service = Java_To_WStr(ap_jenv, a_service);
            std::wstring l_char = Java_To_WStr(ap_jenv, a_characteristic);
            MARK;
            if (m_id_to_bd.find(l_id) == m_id_to_bd.end())
            {
                DEBUG_TRACE("Failed to find device " << l_id);
                return false;
            }
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_bd[l_id]->mp_di;
            if (!lp_di)
            {
                DEBUG_TRACE("No device information for " << l_id);
                return false;
            }
            if (m_id_to_bd[l_id]->m_uuid_to_service.find(l_service) == m_id_to_bd[l_id]->m_uuid_to_service.end())
            {
                DEBUG_TRACE("No service information for " << l_id << '/' << l_service);
                return false;
            }
            BLEService* lp_bs = m_id_to_bd[l_id]->m_uuid_to_service[l_service];
            if (!lp_bs)
            {
                DEBUG_TRACE("No BLE service for " << l_id << '/' << l_service);
                return false;
            }
            if (lp_bs->m_uuid_to_characteristic.find(l_char) == lp_bs->m_uuid_to_characteristic.end())
            {
                DEBUG_TRACE("No characteristic for " << l_id << '/' << l_service << '/' << l_char);
                return false;
            }
            GattCharacteristic* lp_gc = lp_bs->m_uuid_to_characteristic[l_char];
            if (!lp_gc)
            {
                DEBUG_TRACE("No Gatt characteristic for " << l_id << '/' << l_service << '/' << l_char);
                return false;
            }

            // If we're not already registered, can't unwatch!
            if (m_uuid_to_notification_token.find(l_char) == m_uuid_to_notification_token.end())
            {
                DEBUG_TRACE("Not registered for changes on " << l_id << '/' << l_service << '/' << l_char);
                return true;
            }
            if (!WriteClientCharacteristicConfigurationDescriptorAsync(lp_gc, GattClientCharacteristicConfigurationDescriptorValue::None).get())
            {
                DEBUG_TRACE("Failed to write ccd for " << l_id << '/' << l_service << '/' << l_char);
                return false;
            }
            DEBUG_TRACE("Unregistered " << l_id << '/' << l_service << '/' << l_char);
            m_uuid_to_notification_characteristic.erase(l_char);

            lp_gc->ValueChanged(m_uuid_to_notification_token[l_char]);
            m_uuid_to_notification_token.erase(l_char);

            while (!m_uuid_to_changed_value[l_char].empty())
            {
                delete m_uuid_to_changed_value[l_char].back();
                m_uuid_to_changed_value[l_char].pop();
            }
            m_uuid_to_changed_value.erase(l_char);
            MARK;
            m_uuid_to_notification_event[l_char].reset();
            m_uuid_to_notification_event.erase(l_char);
            MARK;

            return true;
        }

};

Discovery *Discovery::smp_dc = NULL;

JNIEXPORT jobjectArray JNICALL Java_javelin_javelin_listBLEDevices
(JNIEnv *ap_jenv, jclass)
{
    Discovery *lp_dc = Discovery::getDiscovery();
    lp_dc->start_discovery(ap_jenv);

    if (lp_dc->wait_for_discovery_complete())
    {
        return lp_dc->getJavaBLEDeviceList(ap_jenv);
    }
    else
    {
        return NULL;
    }
}

JNIEXPORT jobjectArray JNICALL Java_javelin_javelin_listBLEDeviceServices
(JNIEnv *ap_jenv, jclass, jstring a_id)
{
    Discovery* lp_dc = Discovery::getDiscovery();
    return lp_dc->getJavaBLEDeviceServices(ap_jenv, a_id);
}

JNIEXPORT jstring JNICALL Java_javelin_javelin_getBLEDeviceName
(JNIEnv *ap_jenv, jclass, jstring a_id)
{
    std::wstring l_id = Java_To_WStr(ap_jenv, a_id);
    std::wstring l_name = Discovery::getDiscovery()->getName(l_id);

    jstring l_str;
    l_str = ap_jenv->NewString((jchar*)l_name.c_str(), (jsize)l_name.length());

    return l_str;
}

JNIEXPORT jobjectArray JNICALL Java_javelin_javelin_listBLEServiceCharacteristics
(JNIEnv *ap_jenv, jclass, jstring a_id, jstring a_service)

{
    Discovery* lp_dc = Discovery::getDiscovery();
    return lp_dc->getJavaBLEServiceCharacteristics(ap_jenv, a_id, a_service);
}

JNIEXPORT jbyteArray JNICALL Java_javelin_javelin_getBLECharacteristicValue
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

JNIEXPORT jboolean JNICALL Java_javelin_javelin_setBLECharacteristicValue
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
    MARK;
    return lp_dc->setJavaBLECharacteristicValue(ap_jenv, a_id, a_service, a_characteristic, l_dw);
}

JNIEXPORT jboolean JNICALL Java_javelin_javelin_watchBLECharacteristicChanges
(JNIEnv* ap_jenv, jclass, jstring a_id, jstring a_service, jstring a_characteristic)
{
    Discovery* lp_dc = Discovery::getDiscovery();
    return lp_dc->watchJavaBLECharacteristicChanges(ap_jenv, a_id, a_service, a_characteristic);
}

JNIEXPORT jboolean JNICALL Java_javelin_javelin_clearBLECharacteristicChanges
(JNIEnv* ap_jenv, jclass, jstring a_id, jstring a_service, jstring a_characteristic)
{
    Discovery* lp_dc = Discovery::getDiscovery();
    return lp_dc->clearJavaBLECharacteristicChanges(ap_jenv, a_id, a_service, a_characteristic);
}

JNIEXPORT jbyteArray JNICALL Java_javelin_javelin_waitForBLECharacteristicChanges
(JNIEnv* ap_jenv, jclass, jstring a_id, jstring a_service, jstring a_characteristic, jint a_timeout_ms)
{
    Discovery* lp_dc = Discovery::getDiscovery();
    return lp_dc->waitForJavaBLECharacteristicChanges(ap_jenv, a_id, a_service, a_characteristic, a_timeout_ms);
}

JNIEXPORT jboolean JNICALL Java_javelin_javelin_unWatchBLECharacteristicChanges
(JNIEnv* ap_jenv, jclass, jstring a_id, jstring a_service, jstring a_characteristic)
{
    Discovery* lp_dc = Discovery::getDiscovery();
    MARK;
    return lp_dc->unWatchJavaBLECharacteristicChanges(ap_jenv, a_id, a_service, a_characteristic);
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
        MARK;
        Discovery::releaseDiscovery();
        MARK;
        break;
    }
    return TRUE;
}

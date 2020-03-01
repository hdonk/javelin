#include "pch.h"
#include "javelin.h"

#include <../javelin_test/bin/javelin_test_javelin.h>

#include <iostream>
#include <concrt.h>

using namespace winrt;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation;

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

    public:
        Discovery()
            : m_lock_std()
            , m_id_to_di()
            , m_discovery_complete()
        {
            m_discovery_complete.reset();
        };

        void add_di(std::wstring &ar_id, Windows::Devices::Enumeration::DeviceInformation ar_di)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            m_id_to_di[ar_id] = ::new Windows::Devices::Enumeration::DeviceInformation(ar_di);
            Windows::Devices::Enumeration::DeviceInformation *lp_di = m_id_to_di[ar_id];
            std::wcout << __func__ << ":" << std::endl;
            display_di(lp_di);
        }

        void update_di(std::wstring& ar_id, Windows::Devices::Enumeration::DeviceInformationUpdate ar_diu)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            m_id_to_di[ar_id]->Update(ar_diu);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_di[ar_id];
            std::wcout << __func__ << ":" << std::endl;
            display_di(lp_di);
        }

        void remove_di(std::wstring& ar_id)
        {
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);
            Windows::Devices::Enumeration::DeviceInformation* lp_di = m_id_to_di[ar_id];
            std::wcout << __func__ << ":" << std::endl;
            display_di(lp_di);
            ::delete m_id_to_di[ar_id];
            m_id_to_di.erase(ar_id);
        }

        static void display_di(Windows::Devices::Enumeration::DeviceInformation* ap_di)
        {
            std::wstring l_id(ap_di->Id().c_str());
            std::wstring l_name(ap_di->Name().c_str());
            std::wcout << "Id: " << l_id << std::endl;
            std::wcout << "Name: " << l_name << std::endl;
        }

        void start_discovery()
        {
            clear_discovery_complete();

            auto requestedProperties = single_threaded_vector<hstring>({ L"System.Devices.Aep.DeviceAddress", L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.Bluetooth.Le.IsConnectable" });
            std::cout << "1" << std::endl;
            // BT_Code: Example showing paired and non-paired in a single query.
            hstring aqsAllBluetoothLEDevices = L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";
            std::cout << "2" << std::endl;
            m_deviceWatcher =
                Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
                    aqsAllBluetoothLEDevices,
                    requestedProperties,
                    DeviceInformationKind::AssociationEndpoint);
            std::cout << "3" << std::endl;
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

        jobjectArray getJavaBLEDeviceList(JNIEnv* a_jenv)
        { 
            std::map<std::wstring, Windows::Devices::Enumeration::DeviceInformation*>::iterator l_begin, l_end, l_ptr;
            concurrency::critical_section::scoped_lock l_lock(m_lock_std);

            jstring l_str;
            jobjectArray l_ids = 0;
            jsize l_len = (jsize)m_id_to_di.size();

            l_ids = a_jenv->NewObjectArray(l_len, a_jenv->FindClass("java/lang/String"), 0);

            int l_count = 0;
            for (l_begin = m_id_to_di.begin(), l_end = m_id_to_di.end(), l_ptr=l_begin; l_ptr!=l_end; ++l_ptr)
            {
                std::wstring l_id( (l_ptr->second)->Id().c_str());
                l_str = a_jenv->NewString((jchar *)l_id.c_str(), (jsize)l_id.length());
                a_jenv->SetObjectArrayElement(l_ids, l_count++, l_str);
            }

            return l_ids;
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
(JNIEnv *a_jenv, jclass)
{
	std::cout << "Hello from JNI C++" << std::endl;

    Discovery *lp_dc = Discovery::getDiscovery();
    lp_dc->start_discovery();

    std::cout << "5 - Waiting" << std::endl;
    if (lp_dc->wait_for_discovery_complete())
    {
        std::cout << "6 - Finished" << std::endl;

        return lp_dc->getJavaBLEDeviceList(a_jenv);
    }
    else
    {
        std::cout << "7 - Timed out" << std::endl;
        return NULL;
    }
}

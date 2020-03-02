package javelin_test;

public class javelin_test {
	public static javelin_test m_jt = null;
	public javelin_test()
	{
		System.out.println("Hello");
		String l_devices[] = javelin.listBLEDevices();
		System.out.println("Back in Java:");
		if(l_devices != null)
		{
			System.out.println("Devices: ");
			for(String l_device: l_devices)
			{
				System.out.println(" "+l_device);
				String l_name = javelin.getBLEDeviceName(l_device);
				System.out.println("  Name: "+l_name);
				String l_services[] = javelin.listBLEDeviceServices(l_device);
				if(l_services!=null)
				{
					for(String l_service: l_services)
					{
						System.out.println("   Service: "+l_service);
					}
				}
			}
		}
		else
		{
			System.out.println("No devices returned");
		}
		System.out.println("Bye");
	}
	
	public static void main(String[] args)
	{
		System.loadLibrary("msvcp140d_app");
		System.loadLibrary("vcruntime140_1d_app");
		System.loadLibrary("VCRUNTIME140D_APP");
		System.loadLibrary("CONCRT140D_APP");
		System.loadLibrary("ucrtbased");
		System.loadLibrary("api-ms-win-core-synch-l1-2-0");
		System.loadLibrary("api-ms-win-core-synch-l1-1-0");
		System.loadLibrary("api-ms-win-core-processthreads-l1-1-0");
		System.loadLibrary("api-ms-win-core-debug-l1-1-0");
		System.loadLibrary("api-ms-win-core-errorhandling-l1-1-0");
		System.loadLibrary("api-ms-win-core-string-l1-1-0");
		System.loadLibrary("api-ms-win-core-profile-l1-1-0");
		System.loadLibrary("api-ms-win-core-sysinfo-l1-1-0");
		System.loadLibrary("api-ms-win-core-interlocked-l1-1-0");
		System.loadLibrary("api-ms-win-core-winrt-l1-1-0");
		System.loadLibrary("api-ms-win-core-heap-l1-1-0");
		System.loadLibrary("api-ms-win-core-memory-l1-1-0");
		System.loadLibrary("api-ms-win-core-libraryloader-l1-2-0");
		System.loadLibrary("OLEAUT32");
		System.loadLibrary("javelin");
		m_jt = new javelin_test();
	}
}

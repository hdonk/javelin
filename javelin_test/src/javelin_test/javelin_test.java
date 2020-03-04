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
				if(l_name.startsWith("E3DT"))
				{
					String l_services[] = javelin.listBLEDeviceServices(l_device);
					if(l_services!=null)
					{
						for(String l_service: l_services)
						{
							System.out.println("   Service: "+l_service);
							String l_chars[] = javelin.listBLEServiceCharacteristics(l_device, l_service);
							if(l_chars!=null)
							{
								for(String l_char: l_chars)
								{
									System.out.println("   Characteristic: "+l_char);
								}
							}
						}
					}
					byte[] l_bytes = { 0x7f, 0x04 };
					System.out.println("Watch said: "+
						javelin.watchBLECharacteristicChanges(l_device,
								"544d4f54-4f52-2053-4552-564943452020".toUpperCase(),
								"4d4f544f-5220-4d4d-4f44-452020202020".toUpperCase()) );
					System.out.println("Set char said: "+javelin.setBLECharacteristicValue(l_device,
							"544d4f54-4f52-2053-4552-564943452020".toUpperCase(),
							"4d4f544f-5220-4950-4f53-202020202020".toUpperCase(),
							l_bytes));
/*					System.out.println("Clear said: "+
							javelin.clearBLECharacteristicChanges(l_device,
									"544d4f54-4f52-2053-4552-564943452020".toUpperCase(),
									"4d4f544f-5220-4d4d-4f44-452020202020".toUpperCase()) );*/
					System.out.println("Wait 1 for said: "+
							javelin.waitForBLECharacteristicChanges(l_device,
									"544d4f54-4f52-2053-4552-564943452020".toUpperCase(),
									"4d4f544f-5220-4d4d-4f44-452020202020".toUpperCase(), 20000));
					System.out.println("Wait 2 for said: "+
							javelin.waitForBLECharacteristicChanges(l_device,
									"544d4f54-4f52-2053-4552-564943452020".toUpperCase(),
									"4d4f544f-5220-4d4d-4f44-452020202020".toUpperCase(), 20000));
/*					l_bytes = javelin.getBLECharacteristicValue(l_device,
							"544d4f54-4f52-2053-4552-564943452020".toUpperCase(),
							"4d4f544f-5220-4950-4f53-202020202020".toUpperCase());
					System.out.print("Get char said:");
					if(l_bytes!=null) for(byte l_byte : l_bytes)
					{
						System.out.print(" "+l_byte);
					}
					System.out.println("");*/
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

package javelin_test;

public class javelin_test {
	public static javelin_test m_jt = null;
	public javelin_test()
	{
		System.out.println("Hello");
		javelin.listBLEDevices();
	}
	
	public static void main(String[] args)
	{
		System.loadLibrary("msvcp140d_app");
		System.loadLibrary("vcruntime140_1d_app");
		System.loadLibrary("VCRUNTIME140D_APP");
		System.loadLibrary("ucrtbased");
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
		System.loadLibrary("javelin");
		m_jt = new javelin_test();
	}
}

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
		System.loadLibrary("javelin");
		m_jt = new javelin_test();
	}
}

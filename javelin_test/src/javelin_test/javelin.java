package javelin_test;

public class javelin {
	public static native String[] listBLEDevices();
	public static native String getBLEDeviceName(String a_dev_id);
	public static native String[] listBLEDeviceServices(String a_dev_id);
	public static native String[] listBLEServiceCharacteristics(String a_dev_id, String a_service_uuid);
	public static native long getBLECharacteristicValue(String a_dev_id, String a_service_uuid, String a_characterics_uuid);
	public static native boolean setBLECharacteristicValue(String a_dev_id, String a_service_uuid, String a_characterics_uuid, long a_value);
}

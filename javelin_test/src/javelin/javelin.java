package javelin;

public class javelin {
	public static native String[] listBLEDevices();
	public static native String getBLEDeviceName(String a_dev_id);
	public static native String[] listBLEDeviceServices(String a_dev_id);
	public static native String[] listBLEServiceCharacteristics(String a_dev_id, String a_service_uuid);
	public static native byte[] getBLECharacteristicValue(String a_dev_id, String a_service_uuid, String a_characterics_uuid);
	public static native boolean setBLECharacteristicValue(String a_dev_id, String a_service_uuid, String a_characterics_uuid, byte[] a_value);
	
	public static native boolean watchBLECharacteristicChanges(String a_dev_id, String a_service_uuid, String a_characterics_uuid);
	public static native boolean clearBLECharacteristicChanges(String a_dev_id, String a_service_uuid, String a_characterics_uuid);
	public static native byte[] waitForBLECharacteristicChanges(String a_dev_id, String a_service_uuid, String a_characterics_uuid,
			int a_timeout_ms);
	public static native boolean unWatchBLECharacteristicChanges(String a_dev_id, String a_service_uuid, String a_characterics_uuid);
}

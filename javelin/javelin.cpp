#include "pch.h"
#include "javelin.h"

#include "../javelin_test/bin/javelin_test_javelin.h"

#include <iostream>

JNIEXPORT void JNICALL Java_javelin_1test_javelin_listBLEDevices
(JNIEnv*, jclass)
{
	std::cout << "Hello from JNI C++" << std::endl;
}
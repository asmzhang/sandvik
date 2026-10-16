#include "test_helloworld.h"
#include <stdio.h>

JNIEXPORT jstring JNICALL Java_TestJNIHelloWorld_nativeHello(JNIEnv* env, jobject thiz, jstring name) {
    const char* n = (*env)->GetStringUTFChars(env, name, NULL);
    char buff[256];
    int size = (int)(*env)->GetStringUTFLength(env, name);
    snprintf(buff, sizeof(buff), "Hello %s %d", n, size);
    (*env)->ReleaseStringUTFChars(env, name, n);
    return (*env)->NewStringUTF(env, buff);
}

/*
 * Reports the JavaVM that the VM hands to JNI_OnLoad. Real libraries call
 * vm->GetEnv() from here, so a NULL argument makes them fail immediately.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    if (vm == NULL) {
        fprintf(stderr, "[testjni] JNI_OnLoad: JavaVM is NULL\n");
        return JNI_VERSION_1_6;
    }
    JNIEnv* env = NULL;
    jint rc = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    fprintf(stderr, "[testjni] JNI_OnLoad: JavaVM=%p GetEnv rc=%d env=%p\n", (void*)vm, (int)rc, (void*)env);
    return JNI_VERSION_1_6;
}

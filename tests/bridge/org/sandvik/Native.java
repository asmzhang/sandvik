/*
 * This file is part of Sandvik project.
 * Copyright (C) 2025 Christophe Duvernois
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

package org.sandvik;

/**
 * Thin JNI binding of the sandvik embedding API (src/api/sandvik_c.h).
 *
 * The host JVM loads libsandvik and drives the Dalvik VM living inside it:
 * create() gives an opaque handle, the load* methods feed it runtime jars and
 * dex/apk files, callStatic() executes a method of those dex files and hands
 * back its value.
 *
 * The library is located with -Dsandvik.library=<path to libsandvik.dll> or,
 * when the property is absent, through the normal library search path.
 */
public final class Native {
	public static final int KIND_NULL = 0;
	public static final int KIND_STRING = 1;
	public static final int KIND_INT = 2;
	public static final int KIND_LONG = 3;
	public static final int KIND_DOUBLE = 4;
	public static final int KIND_BOOL = 5;
	public static final int KIND_OBJECT = 6;

	static {
		String library = System.getProperty("sandvik.library", "libsandvik.dll");
		System.load(library);
	}

	private Native() {
	}

	/** Create a VM instance. @return opaque handle */
	public static native long create();

	/** Destroy a VM instance created by create(). */
	public static native void destroy(long handle);

	/** 0 = none, 1 = fatal, 2 = error, 3 = warn, 4 = info, 5 = debug. */
	public static native void setLogLevel(long handle, int level);

	/**
	 * 0 = none, 1 = fatal, 2 = error, 3 = warn, 4 = info, 5 = debug.
	 *
	 * The VM prints a banner while it is constructed, so call this one before
	 * create() when the host wants a silent library.
	 */
	public static native void setGlobalLogLevel(int level);

	public static native int loadRtDir(long handle, String dir);

	public static native int loadRtFile(long handle, String file);

	public static native int loadDex(long handle, String path);

	public static native int loadApk(long handle, String path);

	public static native int addClassPath(long handle, String path);

	public static native int setProperty(long handle, String name, String value);

	public static native String getProperty(long handle, String name);

	/**
	 * Call a static method of the dex files loaded in the VM.
	 *
	 * @param handle    VM handle
	 * @param className dotted or slashed class name
	 * @param method    method name
	 * @param signature JNI signature, e.g. "(Ljava/lang/String;)Ljava/lang/String;"
	 * @param args      typed tokens: "s:text", "i:42", "l:42", "d:1.5", "n:"
	 * @return { Integer kind, String text, Long number, String className }
	 */
	public static native Object[] callStatic(long handle, String className, String method, String signature, String[] args);

	/** Last error message of the VM, or null. */
	public static native String lastError(long handle);
}

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
 * M2 acceptance test of the bridge (UNIDBG_EVAL.md 11.3): classes are resolvable
 * by name and, more importantly, objects keep their *identity* while they cross
 * the boundary. A JNI bridge is only usable if the jobject it hands to native
 * code and the object it gets back later are the same object, so that is what
 * this test checks: the handle of an object that travels host -> VM -> host must
 * be the handle it started with.
 *
 * Run: java -Dsandvik.library=<path to libsandvik> -cp build/bridge-classes org.sandvik.BridgeObjects <repo root>
 */
public final class BridgeObjects {
	private BridgeObjects() {
	}

	public static void main(String[] args) {
		final String repo = args.length > 0 ? args[0] : ".";
		Native.setGlobalLogLevel(0);

		final long vm = Native.create();
		check(vm != 0, "sv_vm_create returns a handle");
		check(Native.loadRtFile(vm, repo + "/android/12.0.0/bin/core-oj.dex.jar") == 0, "load core-oj.dex.jar");
		check(Native.loadRtFile(vm, repo + "/android/12.0.0/bin/core-libart.dex.jar") == 0, "load core-libart.dex.jar");
		check(Native.loadRtFile(vm, repo + "/android/12.0.0/bin/icu-stubs.dex.jar") == 0, "load icu-stubs.dex.jar");
		check(Native.loadDex(vm, repo + "/tests/java/bridge/classes.dex") == 0, "load tests/java/bridge/classes.dex");

		// a class can be resolved without being called
		final long hello = Native.resolveClass(vm, "BridgeHello");
		check(hello != 0, "resolveClass(BridgeHello)");
		check(Native.objectIsClass(vm, hello) == 1, "the class handle denotes a class");
		System.out.println("[1] resolveClass(BridgeHello) -> handle " + hello + " (" + Native.objectClassName(vm, hello) + ")");

		// an object created by the host lives in the VM
		final long world = Native.newString(vm, "world");
		check(world != 0, "newString(world)");
		check(Native.objectIsString(vm, world) == 1, "the string handle denotes a String");
		check(Native.objectIsNull(vm, world) == 0, "the string handle is not null");
		check("java.lang.String".equals(Native.objectClassName(vm, world)), "String handle class name");
		check("world".equals(Native.objectAsString(vm, world)), "String handle text");
		System.out.println("[2] newString(\"world\") -> handle " + world + " (" + Native.objectClassName(vm, world) + ")");

		// identity: the object that goes in is the object that comes out
		final Object[] echoed = Native.callStaticHandle(vm, hello, "echo", "(Ljava/lang/Object;)Ljava/lang/Object;",
				new String[] { null }, new long[] { world });
		final long returned = ((Long) echoed[4]).longValue();
		check(returned == world, "echo() hands back the very same handle (expected handle " + world + ", got " + returned + ")");
		System.out.println("[3] BridgeHello.echo(world) -> handle " + returned + " (identity preserved)");

		// a call that produces a new object, and the runtime class of a crossing object
		final Object[] greeted = Native.callStaticHandle(vm, hello, "hello", "(Ljava/lang/String;)Ljava/lang/String;",
				new String[] { null }, new long[] { world });
		check(((Integer) greeted[0]).intValue() == Native.KIND_STRING, "hello returns a String");
		check("hello, world".equals(greeted[1]), "hello returns \"hello, world\"");
		final long greeting = ((Long) greeted[4]).longValue();
		check(greeting != 0 && greeting != world, "hello() produced a new object (handle " + greeting + ")");
		check("hello, world".equals(Native.objectAsString(vm, greeting)), "the new object carries the text");
		System.out.println("[4] BridgeHello.hello(world) -> handle " + greeting + " = \"" + greeted[1] + "\"");

		// arrays keep the identity of their elements, and a null element stays null
		final Object[] wrapped = Native.callStaticHandle(vm, hello, "wrap", "(Ljava/lang/Object;)[Ljava/lang/Object;",
				new String[] { null }, new long[] { greeting });
		final long array = ((Long) wrapped[4]).longValue();
		check(Native.objectIsArray(vm, array) == 1, "wrap() returns an array");
		check(Native.objectArrayLength(vm, array) == 2, "wrap() returns 2 elements, got " + Native.objectArrayLength(vm, array));
		check(Native.objectElement(vm, array, 0) == greeting, "array element 0 is the same handle");
		check(Native.objectIsNull(vm, Native.objectElement(vm, array, 1)) == 1, "array element 1 is null");
		System.out.println("[5] BridgeHello.wrap(greeting) -> array handle " + array + ", element 0 = handle " + Native.objectElement(vm, array, 0));

		// identity is stable across calls (the handle table does not grow per call)
		final Object[] again = Native.callStaticHandle(vm, hello, "echo", "(Ljava/lang/Object;)Ljava/lang/Object;",
				new String[] { null }, new long[] { world });
		check(((Long) again[4]).longValue() == world, "identity is stable across calls");
		final int before = Native.handleCount(vm);
		final Object[] twice = Native.callStaticHandle(vm, hello, "echo", "(Ljava/lang/Object;)Ljava/lang/Object;",
				new String[] { null }, new long[] { greeting });
		check(((Long) twice[4]).longValue() == greeting, "the second object round trips too");
		check(Native.handleCount(vm) == before, "a round trip allocates no new handle");

		// releasing a handle drops it, the VM objects stay alive
		Native.releaseHandle(vm, world);
		check(Native.handleCount(vm) == before - 1, "releaseHandle drops exactly one handle");
		check(Native.objectAsString(vm, greeting) != null, "the other objects are still usable");

		Native.destroy(vm);
		System.out.println("[ok] M2 acceptance passed");
	}

	private static void check(boolean condition, String what) {
		if (!condition) {
			throw new IllegalStateException("FAILED: " + what);
		}
		System.out.println("  [ok] " + what);
	}
}

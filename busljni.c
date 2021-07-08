/** @file busljni.c
 * JNI (Java) interface for BUSL.
 * Copyright (c) 2003-2009, Jan Nijtmans. All rights reserved.
 */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <jni.h>

#if defined(_WIN32) || defined(_WIN64)
#   include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "busl.h"

typedef struct msgdata {
	jmethodID mid /* if non-null, contains the method id of the "write" method */;
	JNIEnv *env; /* java environment */
	jobject obj; /* contains either 'this' (if mid==0) or the OutputStream object (mid!=0)*/
	jclass cls; /* class object of Busl class */
} msgdata;

static const char DATAFIELD[] = "data";
static const char DATATYPE[] = "J";
static const char OUTPUTFIELD[] = "output";
static const char OUTPUTTYPE[] = "Ljava/io/OutputStream;";
static const char WRITEFIELD[] = "write";
static const char WRITETYPE[] = "([B)V";

/**
 * special print function which prints a byte array to a java.io.OutputStream
 */
static void __stdcall wrt(void *output, const char *str) {
	msgdata *d = (msgdata *) output;
	if (d && d->obj) {
		JNIEnv *env = d->env;
		jobject obj = d->obj;
		char ba[BUFSIZE];
		jsize size;
		jbyteArray a;
		sprintf(ba, "%s", str);
		size = (jint) strlen(ba);
		a = (*env)->NewByteArray(env, size);
		(*env)->SetByteArrayRegion(env, a, 0, size, (jbyte *) ba);
		if (!d->mid) {
			/* First call of myprintf. Get output object */
			jfieldID fid = (*env)->GetFieldID(env, d->cls, OUTPUTFIELD, OUTPUTTYPE);
			obj = d->obj = (*env)->GetObjectField(env, obj, fid);
			if (obj) {
				/* if output object found, get the "write" member function */
				jclass cls = (*env)->GetObjectClass(env, obj);
				d->mid = (*env)->GetMethodID(env, cls, WRITEFIELD, WRITETYPE);
			}
		}
		if (d->mid) {
			(*env)->CallVoidMethod(env, obj, d->mid, a);
		}
		if ((*env)->ExceptionOccurred(env)) {
			d->obj = 0;
		}
	}
}

/*
 * Class:     org_tigris_busl_Busl
 * Method:    init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_tigris_busl_Busl_init(JNIEnv *env, jobject obj) {
	jclass cls = (*env)->GetObjectClass(env, obj);
	jfieldID fid = (*env)->GetFieldID(env, cls, DATAFIELD, DATATYPE);
	Busl *s = (Busl *) (size_t) (*env)->GetLongField(env, obj, fid);
	if (!s) {
		s = (Busl *) malloc(sizeof(Busl) + sizeof(msgdata));
		(*env)->SetLongField(env, obj, fid, (jlong) (size_t) s);
	}
	busl_create(s, wrt, 0);
	s->output = 0;
}

/*
 * Class:     org_tigris_busl_Busl
 * Method:    usage
 * Signature: ([B)Z
 */
JNIEXPORT jboolean JNICALL Java_org_tigris_busl_Busl_usage(JNIEnv *env, jobject obj, jbyteArray arg) {
	jboolean result;
	msgdata d = {0, 0, 0, 0};
	jsize size;
	jbyte *ba;
	char *str;
	jfieldID fid;
	Busl *s;
	d.env = env;
	d.obj = obj;
	d.cls = (*env)->GetObjectClass(env, obj);
	fid = (*env)->GetFieldID(env, d.cls, DATAFIELD, DATATYPE);
	s = (Busl *) (size_t) (*env)->GetLongField(env, obj, fid);
	size = (*env)->GetArrayLength(env, arg);
	ba = (*env)->GetByteArrayElements(env, arg, 0);
	str = (char *) malloc(size+1);
	memcpy(str, ba, size);
	str[size] = '\0';
	s->output = &d;
	result = (jboolean) ((busl_usage(s, str)&CHANGED)!=0);
	s->output = 0;
	(*env)->ReleaseByteArrayElements(env, arg, ba, JNI_ABORT);
	free(str);
	return result;
}

/*
 * Class:     org_tigris_busl_Busl
 * Method:    beautify
 * Signature: ([B)Z
 */
JNIEXPORT jboolean JNICALL Java_org_tigris_busl_Busl_beautify(JNIEnv *env, jobject obj, jbyteArray arg) {
	jboolean result;
	msgdata d = {0, 0, 0, 0};
	jsize size;
	jbyte *ba;
	char *str;
	jfieldID fid;
	Busl *s;
	d.env = env;
	d.obj = obj;
	d.cls = (*env)->GetObjectClass(env, obj);
	fid = (*env)->GetFieldID(env, d.cls, DATAFIELD, DATATYPE);
	s = (Busl *) (size_t) (*env)->GetLongField(env, obj, fid);
	size = (*env)->GetArrayLength(env, arg);
	ba = (*env)->GetByteArrayElements(env, arg, 0);
	str = (char *) malloc(size+1);
	memcpy(str, ba, size);
	str[size] = '\0';
	s->output = &d;
	result = (jboolean) ((busl_beautify(s, str)&CHANGED)!=0);
	s->output = 0;
	(*env)->ReleaseByteArrayElements(env, arg, ba, JNI_ABORT);
	free(str);
	return result;
}

/*
 * Class:     org_tigris_busl_Busl
 * Method:    finish
 * Signature: (Z)I
 */
JNIEXPORT jint JNICALL Java_org_tigris_busl_Busl_finish(JNIEnv *env, jobject obj, jboolean changed) {
	jboolean result;
	msgdata d = {0, 0, 0, 0};
	jfieldID fid;
	Busl *s;
	d.env = env;
	d.obj = obj;
	d.cls = (*env)->GetObjectClass(env, obj);
	fid = (*env)->GetFieldID(env, d.cls, DATAFIELD, DATATYPE);
	s = (Busl *) (size_t) (*env)->GetLongField(env, obj, fid);
	s->output = &d;
	result = (jboolean) busl_finish(s, changed);
	s->output = 0;
	return result;
}

/*
 * Class:     org_tigris_busl_Busl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_tigris_busl_Busl_finalize(JNIEnv *env, jobject obj) {
	jclass cls = (*env)->GetObjectClass(env, obj);
	jfieldID fid = (*env)->GetFieldID(env, cls, DATAFIELD, DATATYPE);
	Busl *s = (Busl *) (size_t) (*env)->GetLongField(env, obj, fid);
	if (s) {
		busl_finish(s, CHANGED);
		free((char *) s);
	}
}


#include "cJSON.h"

static cJSON *JsonAddObject(cJSON *parent, const char *key)
{
	cJSON *objectJson = cJSON_CreateObject();
	Assert(!cJSON_IsArray(parent));
	cJSON_AddItemToObject(parent, key, objectJson);
	return objectJson;
}

static cJSON *JsonAddArrayObject(cJSON *parent)
{
	cJSON *objectJson = cJSON_CreateObject();
	Assert(cJSON_IsArray(parent));
	cJSON_AddItemToArray(parent, objectJson);
	return objectJson;
}

static cJSON *JsonAddArray(cJSON *parent, const char *key)
{
	cJSON *arrayJson = cJSON_CreateArray();
	Assert(!cJSON_IsArray(parent)); // NOTE: If we assert here, then we want a seperate 'JsonCreateArrayInArray' function that uses cJSON_AddItemToArray
	cJSON_AddItemToObject(parent, key, arrayJson);
	return arrayJson;
}

static void JsonAddString(cJSON *parent, const char *key, const char *value)
{
	cJSON *stringJson = cJSON_CreateString(value);
	cJSON_AddItemToObject(parent, key, stringJson);
}

static void JsonAddNumber(cJSON *parent, const char *key, r64 value)
{
	cJSON *floatJson = cJSON_CreateNumber(value);
	bool added = cJSON_AddItemToObject(parent, key, floatJson);
	Assert(added);
}
static void JsonAddNumber(cJSON *parent, const char *key, r32 value) { JsonAddNumber(parent, key, (r64)value); }

static void JsonAddNumber(cJSON *parent, const char *key, s8 value) { JsonAddNumber(parent, key, (r64)value); }
static void JsonAddNumber(cJSON *parent, const char *key, s16 value) { JsonAddNumber(parent, key, (r64)value); }
static void JsonAddNumber(cJSON *parent, const char *key, s32 value) { JsonAddNumber(parent, key, (r64)value); }
static void JsonAddNumber(cJSON *parent, const char *key, s64 value) { JsonAddNumber(parent, key, (r64)value); }
static void JsonAddNumber(cJSON *parent, const char *key, u8 value) { JsonAddNumber(parent, key, (r64)value); }
static void JsonAddNumber(cJSON *parent, const char *key, u16 value) { JsonAddNumber(parent, key, (r64)value); }
static void JsonAddNumber(cJSON *parent, const char *key, u32 value) { JsonAddNumber(parent, key, (r64)value); }
static void JsonAddNumber(cJSON *parent, const char *key, u64 value) { JsonAddNumber(parent, key, (r64)value); }

static void JsonAddBool(cJSON *parent, const char *key, bool value)
{
	cJSON *boolJson = cJSON_CreateBool(value);
	cJSON_AddItemToObject(parent, key, boolJson);
}

static void JsonAddVector3(cJSON *parent, const char *key, Vector3 vector)
{
	cJSON *vectorJson = JsonAddObject(parent, key);
	JsonAddNumber(vectorJson, "x", vector.x);
	JsonAddNumber(vectorJson, "y", vector.y);
	JsonAddNumber(vectorJson, "z", vector.z);
}

static void JsonAddColor(cJSON *parent, const char *key, Color color)
{
	cJSON *colorJson = JsonAddObject(parent, key);
	JsonAddNumber(colorJson, "r", color.r);
	JsonAddNumber(colorJson, "g", color.g);
	JsonAddNumber(colorJson, "b", color.b);
	JsonAddNumber(colorJson, "a", color.a);
}

//
//static void SaveLevelToJson(GameData *gameData)
//{
//	cJSON *baseJson = cJSON_CreateObject();
//
//	JsonAddString(baseJson, "levelName", gameData->levelName);
//	JsonAddString(baseJson, "vertexShader", gameData->vertexShader);
//	JsonAddString(baseJson, "fragmentShader", gameData->fragmentShader);
//
//	JsonAddNumber(baseJson, "defaultCameraIndex", gameData->defaultCameraIndex);
//
//	cJSON *camerasArrayJson = JsonAddArray(baseJson, "fixedCameras");
//
//	for (int cameraNum = 0; cameraNum < gameData->fixedCameraCount; ++cameraNum)
//	{
//		FixedCamera *fixedCamera = &gameData->fixedCameras[cameraNum];
//
//		cJSON *cameraJson = JsonAddArrayObject(camerasArrayJson);
//			
//		{
//			Camera *camera = &fixedCamera->camera;
//			JsonAddVector3(cameraJson, "position", camera->position);
//			JsonAddVector3(cameraJson, "target", camera->target);
//			JsonAddVector3(cameraJson, "up", camera->up);
//			JsonAddNumber(cameraJson, "fovy", camera->fovy);
//			JsonAddNumber(cameraJson, "projection", camera->projection);
//		}
//		
//		// Detection zones
//		{
//			cJSON *detectionZonesArrayJson = JsonAddArray(cameraJson, "detectionZones");
//
//			for (u32 zoneNum = 0; zoneNum < fixedCamera->detectionZonesCount; ++zoneNum)
//			{
//				cJSON *detectionZoneJson = JsonAddArrayObject(detectionZonesArrayJson);
//				
//				BoundingBox *detectionZone = &fixedCamera->detectionZones[zoneNum];
//				JsonAddVector3(detectionZoneJson, "min", detectionZone->min);
//				JsonAddVector3(detectionZoneJson, "max", detectionZone->max);
//			}
//		}
//	}
//
//	cJSON *lightArrayJson = JsonAddArray(baseJson, "lights");
//
//	for (u32 lightNum = 0; lightNum < gameData->lightCount; ++lightNum)
//	{
//		Light *light = &gameData->lights[lightNum];
//
//		cJSON *lightJson = JsonAddArrayObject(lightArrayJson);
//		JsonAddBool(lightJson, "enabled", light->enabled);
//		JsonAddNumber(lightJson, "type", light->type);
//		JsonAddVector3(lightJson, "position", light->position);
//		JsonAddVector3(lightJson, "target", light->target);
//		JsonAddColor(lightJson, "color", light->color);
//	}
//	
//	char *jsonString = cJSON_Print(baseJson);
//
//	SaveFileText(gameData->levelName, jsonString);
//
//	free(jsonString);
//	cJSON_Delete(baseJson);
//}

static void JsonReadString(cJSON *parent, const char *key, char *outBuffer)
{
	cJSON *stringJson = cJSON_GetObjectItemCaseSensitive(parent, key);
	Assert(stringJson && cJSON_IsString(stringJson));
	
	int textLengthCopied = TextCopy(outBuffer, stringJson->valuestring);

	int stringLength = TextLength(stringJson->valuestring);
	Assert(textLengthCopied == stringLength);
}

static void JsonReadNumber(cJSON *parent, const char *key, r64 *outNumber)
{
	cJSON *doubleJson = cJSON_GetObjectItemCaseSensitive(parent, key);
	Assert(cJSON_IsNumber(doubleJson));
	*outNumber = doubleJson->valuedouble;
}

#define JSON_READ_NUMBER_MACRO(type, typeMaxSize) \
	r64 doubleValue; \
	JsonReadNumber(parent, key, &doubleValue); \
	Assert(doubleValue <= typeMaxSize); \
	*outNumber = (type)doubleValue;

static void JsonReadNumber(cJSON *parent, const char *key, r32 *outNumber) { JSON_READ_NUMBER_MACRO(r32, R32_MAX) }

static void JsonReadNumber(cJSON *parent, const char *key, u8 *outNumber) { JSON_READ_NUMBER_MACRO(u8, U8_MAX) }
static void JsonReadNumber(cJSON *parent, const char *key, u16 *outNumber) { JSON_READ_NUMBER_MACRO(u16, U16_MAX) }
static void JsonReadNumber(cJSON *parent, const char *key, u32 *outNumber) { JSON_READ_NUMBER_MACRO(u32, U32_MAX) }
static void JsonReadNumber(cJSON *parent, const char *key, u64 *outNumber) { JSON_READ_NUMBER_MACRO(u64, U64_MAX) }
static void JsonReadNumber(cJSON *parent, const char *key, s8 *outNumber) { JSON_READ_NUMBER_MACRO(s8, S8_MAX) }
static void JsonReadNumber(cJSON *parent, const char *key, s16 *outNumber) { JSON_READ_NUMBER_MACRO(s16, S16_MAX) }
static void JsonReadNumber(cJSON *parent, const char *key, s32 *outNumber) { JSON_READ_NUMBER_MACRO(s32, S32_MAX) }
static void JsonReadNumber(cJSON *parent, const char *key, s64 *outNumber) { JSON_READ_NUMBER_MACRO(s64, S64_MAX) }

static void JsonReadBool(cJSON *parent, const char *key, b32 *outBool)
{
	cJSON *boolJson = cJSON_GetObjectItemCaseSensitive(parent, key);
	Assert(boolJson && cJSON_IsBool(boolJson));
	*outBool = (b32)cJSON_IsTrue(boolJson);
}

static void JsonReadVector3(cJSON *parent, const char *key, Vector3 *outVector)
{
	cJSON *vectorJson = cJSON_GetObjectItemCaseSensitive(parent, key);
	Assert(vectorJson && cJSON_IsObject(vectorJson));
	JsonReadNumber(vectorJson, "x", &outVector->x);
	JsonReadNumber(vectorJson, "y", &outVector->y);
	JsonReadNumber(vectorJson, "z", &outVector->z);
}

static void JsonReadColor(cJSON *parent, const char *key, Color *outColor)
{
	cJSON *colorJson = cJSON_GetObjectItemCaseSensitive(parent, key);
	Assert(colorJson && cJSON_IsObject(colorJson));
	JsonReadNumber(colorJson, "r", &outColor->r);
	JsonReadNumber(colorJson, "g", &outColor->g);
	JsonReadNumber(colorJson, "b", &outColor->b);
	JsonReadNumber(colorJson, "a", &outColor->a);
}
//
//static bool ParseLevelFromJson(const char *levelFilePath, GameData *gameData)
//{
//	if (!FileExists(levelFilePath))
//	{
//		TraceLog(LOG_ERROR, "LoadLevelFromJson failed as filepath did not exist - %s\n", levelFilePath);
//		return false;
//	}
//
//	char *jsonString = LoadFileText(levelFilePath);
//
//	cJSON *json = cJSON_Parse(jsonString);
//	if (!json)
//	{
//		const char *jsonError = cJSON_GetErrorPtr();
//		if (jsonError)
//		{
//			TraceLog(LOG_ERROR, "Json file failed to parse file %s - %s\n", levelFilePath, jsonError);
//		}
//		return false;
//	}
//
//	JsonReadString(json, "levelName", gameData->levelName);
//
//	JsonReadString(json, "vertexShader", gameData->vertexShader);
//	JsonReadString(json, "fragmentShader", gameData->fragmentShader);
//
//	JsonReadNumber(json, "defaultCameraIndex", &gameData->defaultCameraIndex);
//
//	cJSON *camerasArrayJson = cJSON_GetObjectItemCaseSensitive(json, "fixedCameras");
//
//	u16 fixedCameraCount = 0;
//
//	cJSON *cameraJson = nullptr;
//	cJSON_ArrayForEach(cameraJson, camerasArrayJson)
//	{
//		if (fixedCameraCount == MAX_CAMERAS)
//		{
//			TraceLog(LOG_ERROR, "Hit MAX_CAMERAS while parsing light json data - cameraCount %d\n", fixedCameraCount);
//			break;
//		}
//
//		FixedCamera *fixedCamera = &gameData->fixedCameras[fixedCameraCount++];
//
//		{
//			Camera *camera = &fixedCamera->camera;
//			JsonReadVector3(cameraJson, "position", &camera->position);
//			JsonReadVector3(cameraJson, "target", &camera->target);
//			JsonReadVector3(cameraJson, "up", &camera->up);
//			JsonReadNumber(cameraJson, "fovy", &camera->fovy);
//			JsonReadNumber(cameraJson, "projection", &camera->projection);
//		}
//
//		// Load detection zones
//		cJSON *detectionZonesArrayJson = cJSON_GetObjectItemCaseSensitive(cameraJson, "detectionZones");
//
//		u16 detectionZonesCount = 0;
//
//		cJSON *detectionZoneJson = nullptr;
//		cJSON_ArrayForEach(detectionZoneJson, detectionZonesArrayJson)
//		{
//			if (detectionZonesCount == MAX_CAMERA_DETECTION_ZONES)
//			{
//				TraceLog(LOG_ERROR, "Hit MAX_CAMERA_DETECTION_ZONES while parsing camera json data - detectionZonesCount %d\n", detectionZonesCount);
//				break;
//			}
//			BoundingBox *detectionZone = &fixedCamera->detectionZones[detectionZonesCount++];
//
//			JsonReadVector3(detectionZoneJson, "min", &detectionZone->min);
//			JsonReadVector3(detectionZoneJson, "max", &detectionZone->max);
//		}
//		fixedCamera->detectionZonesCount = detectionZonesCount;
//	}
//
//	gameData->fixedCameraCount = fixedCameraCount;
//
//	cJSON *lightsArrayJson = cJSON_GetObjectItemCaseSensitive(json, "lights");
//
//	int lightCount = 0;
//
//	cJSON *lightJson = nullptr;
//	cJSON_ArrayForEach(lightJson, lightsArrayJson)
//	{
//		if (lightCount == MAX_LIGHTS)
//		{
//			TraceLog(LOG_ERROR, "Hit MAX_LIGHTS while parsing light json data - lightCount %d\n", lightCount);
//			break;
//		}
//
//		Light *light = &gameData->lights[lightCount++];
//		JsonReadBool(lightJson, "enabled", (b32 *)&light->enabled);
//		JsonReadNumber(lightJson, "type", &light->type);
//		JsonReadVector3(lightJson, "position", &light->position);
//		JsonReadVector3(lightJson, "target", &light->target);
//		JsonReadColor(lightJson, "color", &light->color);
//	}
//
//	gameData->lightCount = lightCount;
//
//	cJSON_Delete(json);
//	free(jsonString);
//
//	return true;
//}
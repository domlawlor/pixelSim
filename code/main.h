
#define FILE_NAME_MAX 256

inline float NormalizeInt8(s8 val)
{
	return (val < 0) ? -(float)val / (float)S8_MIN : (float)val / (float)S8_MAX;
}
inline float NormalizeInt16(s16 val)
{
	return (val < 0) ? -(float)val / (float)S16_MIN : (float)val / (float)S16_MAX;
}
inline float NormalizeInt32(s32 val)
{
	return (val < 0) ? -(float)val / (float)S32_MIN : (float)val / (float)S32_MAX;
}
inline float NormalizeInt64(s64 val)
{
	return (val < 0) ? -(float)val / (float)S64_MIN : (float)val / (float)S64_MAX;
}

Vector3 Vec3Zero = { 0.0, 0.0, 0.0 };
Vector3 Vec3XAxis = { 1.0, 0.0, 0.0 };
Vector3 Vec3YAxis = { 0.0, 1.0, 0.0 };
Vector3 Vec3ZAxis = { 0.0, 0.0, 1.0 };


struct GameData
{
	u32 windowWidth;
	u32 windowHeight;

	Vector2 mousePosition;

	float prevMoveDelta;
};
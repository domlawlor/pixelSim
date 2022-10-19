#include "types.h"

#include "raylib/raylib.h"

#include <cstdio> // for printf

#include <stdlib.h>

#define RAYMATH_IMPLEMENTATION
#include "raylib/raymath.h"

#pragma warning( push )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4456 )
#pragma warning( disable : 4996 )
#define RAYGUI_IMPLEMENTATION
#include "raylib/raygui.h"
#include "raylib/darkGuiStyle.h"
#pragma warning( pop )

#include "hash.h"

#include "main.h"

#include "json.cpp"

#if 0
constexpr u32 gScreenWidth = 2500;
constexpr u32 gScreenHeight = 1300;
#else
constexpr u32 gScreenWidth = 1600;
constexpr u32 gScreenHeight = 896;
#endif

constexpr u8 SimPixelScale = 4;
constexpr float gSimFPS = 120.0f;

constexpr u32 gRegionSize = 64;

constexpr u8 DirtyRectBufferCount = 2;


struct DirtyRect
{
	s32 minX;
	s32 maxX;
	s32 minY;
	s32 maxY;
};
constexpr DirtyRect InvalidDirtyRect = {-1.0f, -1.0f, -1.0f, -1.0f};

enum PixelType : u32
{
	NONE = 0,
	SAND = 1 << 0,
	WATER = 1 << 1,
	GAS = 1 << 2,
	STONE = 1 << 3,
};

const char *PixelTypeToString(PixelType type)
{
	const char *typeString = nullptr;
	switch(type)
	{
	case PixelType::NONE: typeString = "None"; break;
	case PixelType::SAND: typeString = "Sand"; break;
	case PixelType::WATER: typeString = "Water"; break;
	case PixelType::GAS: typeString = "Gas"; break;
	case PixelType::STONE: typeString = "Stone"; break;
	default: Assert(false); // Need a string for this type!!
	}
	return typeString;
}

Color GetTypeColor(PixelType type)
{
	Color result = BLANK;

	int randValue = GetRandomValue(0, 100000);

	u32 sandColors[] = {0xf9a31bff, 0xffd541ff, 0xfffc40ff};
	u32 waterColors[] = {0x143464ff, 0x285cc4ff, 0x249fdeff};
	u32 gasColors[] = {0xb3b9d1ff, 0xb3b9d1ff};
	u32 stoneColors[] = {0x333941ff, 0x4a5462ff, 0x6d758dff};

	switch(type)
	{
	case PixelType::SAND:
		result = GetColor(sandColors[randValue % ArrayCount(sandColors)]);
		break;
	case PixelType::WATER:	
		result = GetColor(waterColors[randValue % ArrayCount(waterColors)]);
		break;
	case PixelType::GAS:
		result = GetColor(gasColors[randValue % ArrayCount(gasColors)]);
		break;
	case PixelType::STONE:
		result = GetColor(stoneColors[randValue % ArrayCount(waterColors)]);
		break;
	case PixelType::NONE:
		result = BLACK;
		break;
	default:
		Assert(false); // Type found without a color? Fix this
	}
	return result;
}



inline bool AreDirtyRectsEqual(DirtyRect rectA, DirtyRect rectB)
{
	bool result = (rectA.minX == rectB.minX) &&
		(rectA.minY == rectB.minY) &&
		(rectA.maxX == rectB.maxX) &&
		(rectA.maxY == rectB.maxY);
	return result;
}

inline bool IsInvalidDirtyRect(DirtyRect rect)
{
	return AreDirtyRectsEqual(rect, InvalidDirtyRect);
}

inline bool IsPosInDirtyRect(Vector2 pos, DirtyRect rect)
{
	bool result = pos.x >= rect.minX && pos.x < rect.maxX &&
		pos.y >= rect.minY && pos.y < rect.maxY;
	return result;
}

struct PixelState
{
	u32 lastFrameUpdated;
	PixelType type;
};

#define UPDATE_STAGE_COUNT 4
struct SimUpdateStage
{
	u32 *regionIndicesToUpdate;
	u32 regionIndexCount;
};

class PixelSim
{
public:
	PixelSim(u32 simWidth, u32 simHeight, u32 simPixelScale, u32 regionSize) 
		: m_simWidth(simWidth), m_simHeight(simHeight), m_simPixelScale(simPixelScale), m_regionPixelSize(regionSize)
	{
		m_pixelTotal = m_simWidth * m_simHeight;

		m_pixelStates = (PixelState *)malloc(m_pixelTotal * sizeof(PixelState));
		memset(m_pixelStates, 0, m_pixelTotal * sizeof(PixelState));

		m_pixelBuffer = (Color *)malloc(m_pixelTotal * sizeof(Color));
		memset(m_pixelBuffer, 0, m_pixelTotal * sizeof(Color));

		m_regionColumns = (u32)ceil((r32)m_simWidth / (r32)m_regionPixelSize);
		m_regionRows = (u32)ceil((r32)m_simHeight / (r32)m_regionPixelSize);

		m_regionCount = m_regionColumns * m_regionRows;
		for(int i = 0; i < DirtyRectBufferCount; ++i)
		{
			m_regionDirtyRectBuffers[i] = (DirtyRect *)malloc(m_regionCount * sizeof(DirtyRect));
			ClearRegionDirtyRects(m_regionDirtyRectBuffers[i]);
		}
		m_readRegionBufferIndex = 0;
		m_writeRegionBufferIndex = 1;

		// Split region updates into stages. Each stage will update a set regions in a checkboard pattern
		// To be used by threading to isolate data access for pixels to each thread
		u32 *updateOrderBuffer = (u32 *)malloc(m_regionCount * sizeof(u32));

		
		u32 maxRegionsPerStage = (u32)ceil(m_regionCount / UPDATE_STAGE_COUNT) + 1;
		
		memset(m_stages, 0, UPDATE_STAGE_COUNT * sizeof(SimUpdateStage));
		m_stages[0].regionIndicesToUpdate = updateOrderBuffer + (maxRegionsPerStage * 0);
		m_stages[1].regionIndicesToUpdate = updateOrderBuffer + (maxRegionsPerStage * 1);
		m_stages[2].regionIndicesToUpdate = updateOrderBuffer + (maxRegionsPerStage * 2);
		m_stages[3].regionIndicesToUpdate = updateOrderBuffer + (maxRegionsPerStage * 3);
		
		for(s32 rowNum = (m_regionRows-1); rowNum >= 0; --rowNum) // Add from the bottom of the screen up
		{
			for(s32 colNum = 0; colNum < m_regionColumns; ++colNum)
			{
				SimUpdateStage *stage = nullptr;

				bool colIsEven = ((colNum % 2) == 0);
				bool rowIsEven = ((rowNum % 2) == 0);
				
				if(colIsEven && rowIsEven) { stage = &m_stages[0]; }
				else if(!colIsEven && rowIsEven)  { stage = &m_stages[1]; }
				else if(colIsEven && !rowIsEven)  { stage = &m_stages[2]; }
				else if(!colIsEven && !rowIsEven) { stage = &m_stages[3]; }

				Assert(stage);
				Assert(stage->regionIndexCount < maxRegionsPerStage);

				u32 regionIndex = (rowNum * m_regionColumns) + colNum;
				stage->regionIndicesToUpdate[stage->regionIndexCount] = regionIndex;
				++stage->regionIndexCount;		
			}
		}
	}

	void ClearRegionDirtyRects(DirtyRect *regionDirtyRects)
	{
		for(u32 regionNum = 0; regionNum < m_regionCount; ++regionNum)
		{
			DirtyRect *regionDirtyRect = &regionDirtyRects[regionNum];
			*regionDirtyRect = InvalidDirtyRect;
		}
	}

	void SwapRegionDirtyRectBuffers()
	{
		u8 prevReadIndex = m_readRegionBufferIndex;
		m_readRegionBufferIndex = m_writeRegionBufferIndex;
		m_writeRegionBufferIndex = prevReadIndex;

		// Prep write buffer by clearing
		ClearRegionDirtyRects(m_regionDirtyRectBuffers[m_writeRegionBufferIndex]);
	}

	bool InSimBounds(u32 x, u32 y)
	{
		bool result = (x >= 0 && x < m_simWidth) &&
			(y >= 0 && y < m_simHeight);
		return result;
	}
	bool InSimBounds(Vector2 pos) { return InSimBounds(pos.x, pos.y); }


	inline Vector2 ScreenToSimPos(Vector2 screenPos)
	{
		Vector2 result = {};
		result.x = floor(screenPos.x / m_simPixelScale);
		result.y = floor(screenPos.y / m_simPixelScale);
		return result;
	}

	inline Vector2 SimToScreenPos(Vector2 simPos)
	{
		Vector2 result = {};
		result.x = simPos.x * m_simPixelScale;
		result.y = simPos.y * m_simPixelScale;
		return result;
	}

	void AddToDirtyRect(Vector2 pos)
	{
		u32 regionColumn = floorf(pos.x / m_regionPixelSize);
		u32 regionRow = floorf(pos.y / m_regionPixelSize);
		u32 regionIndex = (regionRow * m_regionColumns) + regionColumn;

		Assert(regionIndex < m_regionCount);
		DirtyRect *regionDirtyRects = m_regionDirtyRectBuffers[m_writeRegionBufferIndex];
		DirtyRect *regionDirtyRect = &regionDirtyRects[regionIndex];

		// If dirty rect has not be initialised, set default values so min/max calculations work
		// min become highests possible values, so width and height of sim
		// max set to zero
		if(IsInvalidDirtyRect(*regionDirtyRect))
		{
			regionDirtyRect->minX = m_simWidth;
			regionDirtyRect->maxX = 0.0f;
			regionDirtyRect->minY = m_simHeight;
			regionDirtyRect->maxY = 0.0f;
		}

		regionDirtyRect->minX = MIN(regionDirtyRect->minX, pos.x - 1);
		regionDirtyRect->maxX = MAX(regionDirtyRect->maxX, pos.x + 1);
		regionDirtyRect->minY = MIN(regionDirtyRect->minY, pos.y - 1);
		regionDirtyRect->maxY = MAX(regionDirtyRect->maxY, pos.y + 1);
	}

	inline Rectangle GetSimSize()
	{
		Rectangle simRectangle = {0,0, (r32)m_simWidth, (r32)m_simHeight};
		return simRectangle;
	}

	inline u32 GetSimScale()
	{
		return m_simPixelScale;
	}

	inline Color *GetPixelBuffer()
	{
		return m_pixelBuffer;
	}

	inline PixelState *GetPixelStatePtr(u32 x, u32 y)
	{
		Assert(InSimBounds(x, y));
		u32 offset = (y * m_simWidth) + x;
		PixelState *outState = m_pixelStates + offset;
		return outState;
	}
	inline PixelState *GetPixelStatePtr(Vector2 pos) { return GetPixelStatePtr(pos.x, pos.y); }


	inline Color GetPixel(u32 x, u32 y)
	{
		Assert(InSimBounds(x, y));
		u32 offset = (y * m_simWidth) + x;
		Color *outPixel = m_pixelBuffer + offset;
		return *outPixel;
	}
	inline Color GetPixel(Vector2 pos) { return GetPixel(pos.x, pos.y); }

	inline void SetPixel(u32 x, u32 y, Color color)
	{
		Assert(InSimBounds(x, y));
		u32 offset = (y * m_simWidth) + x;
		Color *outPixel = m_pixelBuffer + offset;
		*outPixel = color;
	}
	inline void SetPixel(Vector2 pos, Color color) { return SetPixel(pos.x, pos.y, color); }

	void CreatePixel(Vector2 pos, PixelType type)
	{
		if(!InSimBounds(pos))
		{
			return;
		}

		PixelState *state = GetPixelStatePtr(pos);
		if(state->type == PixelType::NONE)
		{
			state->type = type;

			Color color = GetTypeColor(type);
			SetPixel(pos, color);

			AddToDirtyRect(pos);
		}
		else if(type == PixelType::NONE) // If we are trying to create empty pixels. aka erase functionality
		{
			ClearPixel(pos);
		}
	}

	void CreatePixelsInSquare(Vector2 pos, u32 spawnCount, PixelType type)
	{
		s32 halfSideLength = spawnCount / 2;

		u32 pixelCount = spawnCount;
		for(int pixelNum = 0; pixelNum < pixelCount; ++pixelNum)
		{
			int randX = GetRandomValue(-halfSideLength, halfSideLength);
			int randY = GetRandomValue(-halfSideLength, halfSideLength);

			Vector2 randomOffset = {};
			randomOffset.x = randX;
			randomOffset.y = randY;
			Vector2 randomPos = Vector2Add(pos, randomOffset);
			CreatePixel(randomPos, type);
		}
	}

	void CreatePixelsInCircle(Vector2 pos, s32 radius, PixelType type)
	{
		for(s32 y = -radius; y <= radius; y++)
		{
			for(s32 x = -radius; x <= radius; x++)
			{
				if((x * x) + (y * y) <= (radius * radius))
				{
					Vector2 pixelPos = {pos.x + x, pos.y + y};
					CreatePixel(pixelPos, type);
				}
			}
		}
	}

	// For cases like gas when it leaves the area and we want it to disappear
	void ClearPixel(Vector2 srcPos)
	{
		PixelState *srcState = GetPixelStatePtr(srcPos);
		srcState->type = PixelType::NONE;
		SetPixel(srcPos, BLANK);
		AddToDirtyRect(srcPos);
	}

	void MovePixel(Vector2 srcPos, Vector2 destPos)
	{
		PixelState *srcState = GetPixelStatePtr(srcPos);
		PixelState *destState = GetPixelStatePtr(destPos);

		destState->lastFrameUpdated = m_updateFrameNum;
		destState->type = srcState->type;
		srcState->type = PixelType::NONE;

		// move pixel
		Color sourceColor = GetPixel(srcPos);
		SetPixel(srcPos, BLANK);

		SetPixel(destPos, sourceColor);

		AddToDirtyRect(srcPos);
		AddToDirtyRect(destPos);
	}

	void SwapPixels(Vector2 srcPos, Vector2 destPos)
	{
		PixelState *srcState = GetPixelStatePtr(srcPos);
		PixelState *destState = GetPixelStatePtr(destPos);

		PixelType destType = destState->type;

		destState->lastFrameUpdated = m_updateFrameNum;
		destState->type = srcState->type;

		srcState->lastFrameUpdated = m_updateFrameNum;
		srcState->type = destType;

		// move pixel
		Color srcColor = GetPixel(srcPos);
		Color destColor = GetPixel(destPos);
		
		SetPixel(srcPos, destColor);
		SetPixel(destPos, srcColor);

		AddToDirtyRect(srcPos);
		AddToDirtyRect(destPos);
	}

	struct MoveTestResult
	{
		Vector2 lastValidPos;
		Vector2 colliderPos;
		PixelType lastCollisionType;
		bool hitBoundary;
	};

	bool PhysicsMoveTest(Vector2 srcPos, Vector2 destPos, u32 collideBitmask, bool stopAtBoundary, MoveTestResult *testResult)
	{
		Vector2 moveVec = Vector2Subtract(destPos, srcPos);

		float x = srcPos.x;
		float y = srcPos.y;

		float stepX = moveVec.x < 0.0 ? -1.0 : 1.0;
		float stepY = moveVec.y < 0.0 ? -1.0 : 1.0;

		float xLeft = floor(fabs(moveVec.x));
		float yLeft = floor(fabs(moveVec.y));

		Vector2 testPos = srcPos;

		testResult->lastValidPos = srcPos;
		testResult->colliderPos = Vector2Zero();
		testResult->lastCollisionType = PixelType::NONE;
		testResult->hitBoundary = false;

		while(!Vector2Equals(testPos, destPos))
		{
			if(xLeft >= yLeft)
			{
				xLeft -= 1;
				x += stepX;
			}
			else
			{
				yLeft -= 1;
				y += stepY;
			}
			testPos = {x, y};

			bool inBounds = InSimBounds(testPos);
			if(stopAtBoundary && !inBounds)
			{
				testResult->hitBoundary = true;
				break;
			}
			if(inBounds)
			{
				PixelState *testPosState = GetPixelStatePtr(testPos);
				if((testPosState->type & collideBitmask) != 0)
				{
					testResult->colliderPos = testPos;
					testResult->lastCollisionType = testPosState->type;
				}
				if(testResult->lastCollisionType != PixelType::NONE)
				{
					break;
				}
			}
			testResult->lastValidPos = testPos;
		}

		bool canMove = !Vector2Equals(srcPos, testResult->lastValidPos);
		return canMove;
	}

	bool UpdateSand(Vector2 pos)
	{
		bool stopAtBoundary = true;
		u32 collideBitmask = PixelType::SAND | PixelType::STONE;
		s32 velocity = 1;
		s32 xDelta = (rand() % 2) == 0 ? velocity : -velocity;

		Vector2 movePositions[] = {
			{pos.x, pos.y + velocity}, // Down
			{pos.x + xDelta, pos.y + velocity}, // DownLeft or DownRight
			{pos.x - xDelta, pos.y + velocity}, // Opposite of above
		};
		u8 movePositionCount = ArrayCount(movePositions);

		for(u8 posNum = 0; posNum < movePositionCount; ++posNum)
		{
			Vector2 testPos = movePositions[posNum];

			bool inBounds = InSimBounds(testPos);
			if(inBounds)
			{
				PixelState *testPosState = GetPixelStatePtr(testPos);
				if((testPosState->type & collideBitmask) == 0)
				{
					if(testPosState->type == PixelType::WATER)
					{
						SwapPixels(pos, testPos);
					}
					else
					{
						MovePixel(pos, testPos);
					}
					return true;
				}
			}
		}

		return false;
	}

	bool UpdateWater(Vector2 pos)
	{	
		bool stopAtBoundary = true;
		u32 collideBitmask = PixelType::SAND | PixelType::WATER | PixelType::STONE;
		s32 velocity = 5;
		s32 xDelta = (rand() % 2) == 0 ? velocity : -velocity;

		Vector2 movePositions[] = {
			{pos.x, pos.y + velocity}, // Down
			{pos.x + xDelta, pos.y + velocity}, // DownLeft or DownRight
			{pos.x - xDelta, pos.y + velocity}, // Opposite of above
			{pos.x + xDelta, pos.y}, // Left or right
			{pos.x - xDelta, pos.y}, // Oppisite of above
		};
		u8 movePositionCount = ArrayCount(movePositions);

		for(u8 posNum = 0; posNum < movePositionCount; ++posNum)
		{
			Vector2 testPos = movePositions[posNum];

			MoveTestResult moveResult;
			if(PhysicsMoveTest(pos, testPos, collideBitmask, stopAtBoundary, &moveResult))
			{
				MovePixel(pos, moveResult.lastValidPos);
				return true;
			}
		}

		return false;
	}

	bool UpdateGas(Vector2 pos)
	{
		bool stopAtBoundary = false;
		u32 collideBitmask = PixelType::SAND | PixelType::WATER | PixelType::GAS | PixelType::STONE; 
		s32 velocity = 1;
		s32 xDelta = (rand() % 2) == 0 ? velocity : -velocity;

		Vector2 movePositions[] = {
			{pos.x, pos.y - velocity}, // Up
			{pos.x + xDelta, pos.y - velocity}, // UpLeft or UpRight
			{pos.x - xDelta, pos.y - velocity}, // Opposite of above
			{pos.x + xDelta, pos.y}, // Left or right
			{pos.x - xDelta, pos.y}, // Oppisite of above
		};
		u8 movePositionCount = ArrayCount(movePositions);
		
		for(u8 posNum = 0; posNum < movePositionCount; ++posNum)
		{
			Vector2 testPos = movePositions[posNum];

			MoveTestResult moveResult;
			if(PhysicsMoveTest(pos, testPos, collideBitmask, stopAtBoundary, &moveResult))
			{
				if(InSimBounds(moveResult.lastValidPos))
				{
					MovePixel(pos, moveResult.lastValidPos);
				}
				else
				{
					ClearPixel(pos);
				}
				return true;
			}
		}

		return false;
	}

	void UpdateSim(float delta)
	{
		m_updateFrameNum++;

		bool evenFrame = (m_updateFrameNum % 2) == 0;
		u32 startingStageNum = m_updateFrameNum % UPDATE_STAGE_COUNT;

		DirtyRect *regionDirtyRects = m_regionDirtyRectBuffers[m_readRegionBufferIndex];

		for(u32 stageNum = 0; stageNum < UPDATE_STAGE_COUNT; ++stageNum)
		{
			SimUpdateStage *stage = &m_stages[(startingStageNum + stageNum) % UPDATE_STAGE_COUNT];
			for(u32 regionNum = 0; regionNum < stage->regionIndexCount; ++regionNum)
			{
				u32 regionIndex = stage->regionIndicesToUpdate[regionNum];

				DirtyRect dirtyRect = regionDirtyRects[regionIndex];

				if(IsInvalidDirtyRect(dirtyRect))
				{
					continue;
				}

				s32 startX = Clamp(dirtyRect.minX - 1, 0, m_simWidth);
				s32 endX = Clamp(dirtyRect.maxX + 1, 0, m_simWidth);
				s32 startY = Clamp(dirtyRect.minY - 1, 0, m_simHeight);
				s32 endY = Clamp(dirtyRect.maxY + 1, 0, m_simHeight);

				for(s32 y = (endY - 1); y >= startY; --y)
				{
					for(s32 x = evenFrame ? (endX - 1) : startX; evenFrame ? x >= startX : x < endX; evenFrame ? --x : ++x)
					{
						Vector2 pos = {x, y};

						bool moved = false;

						PixelState *state = GetPixelStatePtr(pos);

						if(state->lastFrameUpdated != m_updateFrameNum)
						{
							switch(state->type)
							{
							case PixelType::SAND: { moved = UpdateSand(pos); } break;
							case PixelType::WATER: { moved = UpdateWater(pos); } break;
							case PixelType::GAS: {moved = UpdateGas(pos); } break;
							}
						}
					}
				}
			}
		}

		SwapRegionDirtyRectBuffers();
	}
	
	void DebugDrawRegions(bool drawActiveRegions, bool drawDirtyRects, bool drawRegionNumbers)
	{
		DirtyRect *regionDirtyRects = m_regionDirtyRectBuffers[m_readRegionBufferIndex];

		if(drawActiveRegions || drawRegionNumbers)
		{
			constexpr int TextBufferSize = 256;
			char textBuffer[TextBufferSize] = {};

			for(u32 rowNum = 0; rowNum < m_regionRows; ++rowNum)
			{
				for(u32 colNum = 0; colNum < m_regionColumns; ++colNum)
				{
					u32 regionIndex = (rowNum * m_regionColumns) + colNum;
					DirtyRect regionDirtyRect = regionDirtyRects[regionIndex];

					u32 screenX = (colNum * m_regionPixelSize) * m_simPixelScale;
					u32 screenY = (rowNum * m_regionPixelSize) * m_simPixelScale;

					if(drawRegionNumbers)
					{
						u8 debugFontSize = 20;
						sprintf_s(textBuffer, TextBufferSize, "%u,%u - %u", colNum, rowNum, regionIndex);
						DrawText(textBuffer, screenX, screenY, debugFontSize, WHITE);
					}

					// Draw gray inactive regions first so active regions overlap when drawn
					if(drawActiveRegions)
					{
						if(IsInvalidDirtyRect(regionDirtyRect))
						{
							u32 regionSreenSize = m_regionPixelSize * m_simPixelScale;
							DrawRectangleLines(screenX, screenY, regionSreenSize, regionSreenSize, DARKGRAY);
						}
					}
				}
			}
		}

		for(u32 rowNum = 0; rowNum < m_regionRows; ++rowNum)
		{
			for(u32 colNum = 0; colNum < m_regionColumns; ++colNum)
			{
				u32 regionIndex = (rowNum * m_regionColumns) + colNum;
				DirtyRect regionDirtyRect = regionDirtyRects[regionIndex];
				bool validRect = !IsInvalidDirtyRect(regionDirtyRect);
				if(validRect)
				{
					if(drawActiveRegions)
					{
						u32 screenX = (colNum * m_regionPixelSize) * m_simPixelScale;
						u32 screenY = (rowNum * m_regionPixelSize) * m_simPixelScale;
						u32 regionSreenSize = m_regionPixelSize * m_simPixelScale;
						DrawRectangleLines(screenX, screenY, regionSreenSize, regionSreenSize, WHITE);
					}
					if(drawDirtyRects)
					{
						u32 screenX = regionDirtyRect.minX * m_simPixelScale;
						u32 screenY = regionDirtyRect.minY * m_simPixelScale;
						u32 width = (regionDirtyRect.maxX - regionDirtyRect.minX) * m_simPixelScale;
						u32 height = (regionDirtyRect.maxY - regionDirtyRect.minY) * m_simPixelScale;
						DrawRectangleLines(screenX, screenY, width, height, RED);
					}
				}
			}
		}
	}

private:
	PixelState *m_pixelStates;
	Color *m_pixelBuffer;

	DirtyRect *m_regionDirtyRectBuffers[DirtyRectBufferCount];
	u8 m_readRegionBufferIndex;
	u8 m_writeRegionBufferIndex;

	u32 m_updateFrameNum;

	u32 m_simPixelScale;
	u32 m_simWidth;
	u32 m_simHeight;
	u32 m_pixelTotal;

	u32 m_regionColumns;
	u32 m_regionRows;
	u32 m_regionCount;
	u32 m_regionPixelSize;

	SimUpdateStage m_stages[UPDATE_STAGE_COUNT];
};

#include "time.h"

int main()
{
	SetRandomSeed(time(0));

	SetConfigFlags(FLAG_MSAA_4X_HINT);
	
	GameData gameData = {};

	gameData.windowWidth = gScreenWidth;
	gameData.windowHeight = gScreenHeight;
	InitWindow(gScreenWidth, gScreenHeight, "Test window");

	u8 targetFPS = 120;
	SetTargetFPS(targetFPS);

	u32 simWidth = gScreenWidth / SimPixelScale;
	u32 simHeight = gScreenHeight / SimPixelScale;

	Image blankImage = GenImageColor(simWidth, simHeight, BLANK);
	Assert(blankImage.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

	Texture2D screenTexture = LoadTextureFromImage(blankImage);

	PixelSim pixelSim(simWidth, simHeight, SimPixelScale, gRegionSize);

	float lastFrameTime = GetFrameTime();
	
	float simStepTime = 1.0f / gSimFPS;
	float simTimeAccumulator = 0.0f;

	bool debugKey1Toggle = false;
	bool debugKey2Toggle = false;
	bool debugKey3Toggle = false;
	bool debugKey4Toggle = false;
	bool debugKey5Toggle = false;
	bool debugKey6Toggle = false;
	bool debugKey7Toggle = false;

	PixelType activeSpawnType = PixelType::SAND;

	constexpr u32 MinSpawnPixelCount = 1;
	constexpr u32 MaxSpawnPixelCount = 256;

	u32 spawnPixelCount = MinSpawnPixelCount;

	while(!WindowShouldClose())
	{
		float frameTimeDelta = GetFrameTime() - lastFrameTime;

		Vector2 mousePos = GetMousePosition();
		Vector2 mouseSimPos = pixelSim.ScreenToSimPos(mousePos);
		
		float mouseWheelMovement = GetMouseWheelMove();
		//Vector2 mouseDelta = GetMouseDelta();

		bool shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
		bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);

		if(IsKeyDown(KEY_ONE)) { activeSpawnType = PixelType::SAND; }
		else if(IsKeyDown(KEY_TWO)) { activeSpawnType = PixelType::WATER; }
		else if(IsKeyDown(KEY_THREE)) { activeSpawnType = PixelType::GAS; }
		else if(IsKeyDown(KEY_FOUR)) { activeSpawnType = PixelType::STONE; }
		else if(IsKeyDown(KEY_ZERO)) { activeSpawnType = PixelType::NONE; }

		if(mouseWheelMovement != 0.0f) // TODO Might have to have a margin of error
		{
			if(mouseWheelMovement > 0)
			{
				spawnPixelCount = MIN(spawnPixelCount * 2, MaxSpawnPixelCount);
			}
			else
			{
				spawnPixelCount = MAX(spawnPixelCount * 0.5, MinSpawnPixelCount);
			}
		}

		if(IsMouseButtonDown(MOUSE_LEFT_BUTTON))
		{
			if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) // Place a single pixel if just pressed
			{	
				pixelSim.CreatePixelsInCircle(mouseSimPos, spawnPixelCount, activeSpawnType);
			}	
		}
		else if(IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
		{
			pixelSim.CreatePixelsInCircle(mouseSimPos, spawnPixelCount, activeSpawnType);
		}

		if(IsKeyPressed(KEY_F1)) { debugKey1Toggle = !debugKey1Toggle; }
		if(IsKeyPressed(KEY_F2)) { debugKey2Toggle = !debugKey2Toggle; }
		if(IsKeyPressed(KEY_F3)) { debugKey3Toggle = !debugKey3Toggle; }
		if(IsKeyPressed(KEY_F4)) { debugKey4Toggle = !debugKey4Toggle; }
		if(IsKeyPressed(KEY_F5)) { debugKey5Toggle = !debugKey5Toggle; }
		if(IsKeyPressed(KEY_F6)) { debugKey6Toggle = !debugKey6Toggle; }
		if(IsKeyPressed(KEY_F7)) { debugKey7Toggle = !debugKey7Toggle; }

		simTimeAccumulator += frameTimeDelta;
		if (simTimeAccumulator > simStepTime)
		{
			simTimeAccumulator -= simStepTime;
			pixelSim.UpdateSim(simStepTime);
		}

		Color *pixelBuffer = pixelSim.GetPixelBuffer();
		Rectangle simUpdateRect = pixelSim.GetSimSize();
		UpdateTextureRec(screenTexture, simUpdateRect, pixelBuffer);

		// Draw
		BeginDrawing();
		ClearBackground(BLACK);
		DrawTextureEx(screenTexture, {0,0}, 0, pixelSim.GetSimScale(), WHITE);

		bool drawActiveRegions = debugKey1Toggle;
		bool drawDirtyRects = debugKey2Toggle;
		bool drawRegionNumbers = debugKey3Toggle;
		if(drawActiveRegions || drawDirtyRects)
		{
			pixelSim.DebugDrawRegions(drawActiveRegions, drawDirtyRects, drawRegionNumbers);
		}

		DrawFPS(10, 10);

		//PixelTypeStrings
		u8 debugFontSize = 20;
		Color debugTextColor = GetColor(0x009e2fff);

		constexpr int TextBufferSize = 256;
		char textBuffer[TextBufferSize] = {};
		sprintf_s(textBuffer, TextBufferSize, "Spawn Type - %s", PixelTypeToString(activeSpawnType));
		DrawText(textBuffer, 10, 40, debugFontSize, debugTextColor);


		sprintf_s(textBuffer, TextBufferSize, "Spawn amount - %u", spawnPixelCount);
		DrawText(textBuffer, 10, 60, debugFontSize, debugTextColor);

		EndDrawing();

		
	}
	
	CloseWindow();
	
	return 0;
}

/*******************************************
	TankAssignment.cpp

	Shell scene and game functions
********************************************/

#include <sstream>
#include <string>
using namespace std;

#include <d3d10.h>
#include <d3dx10.h>

#include "Defines.h"
#include "CVector3.h"
#include "Camera.h"
#include "Light.h"
#include "EntityManager.h"
#include "Messenger.h"
#include "CParseLevel.h"
#include "TankAssignment.h"

namespace gen
{

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Control speed
const float CameraRotSpeed = 2.0f;
float CameraMoveSpeed = 80.0f;

// Amount of time to pass before calculating new average update time
const float UpdateTimePeriod = 1.0f;


//-----------------------------------------------------------------------------
// Global system variables
//-----------------------------------------------------------------------------

// Get reference to global DirectX variables from another source file
extern ID3D10Device*           g_pd3dDevice;
extern IDXGISwapChain*         SwapChain;
extern ID3D10DepthStencilView* DepthStencilView;
extern ID3D10RenderTargetView* BackBufferRenderTarget;
extern ID3DX10Font*            OSDFont;

// Actual viewport dimensions (fullscreen or windowed)
extern TUInt32 ViewportWidth;
extern TUInt32 ViewportHeight;

// Current mouse position
extern TUInt32 MouseX;
extern TUInt32 MouseY;

// Messenger class for sending messages to and between entities
extern CMessenger Messenger;


//-----------------------------------------------------------------------------
// Global game/scene variables
//-----------------------------------------------------------------------------

// Entity manager
CEntityManager EntityManager;
CParseLevel LevelParser(&EntityManager);

// Tank UIDs
const int numTanks = 6;
TEntityUID Tanks[numTanks];

// Other scene elements
const int NumLights = 2;
CLight*  Lights[NumLights];
SColourRGBA AmbientLight;
CCamera* MainCamera;

// Sum of recent update times and number of times in the sum - used to calculate
// average over a given time period
float SumUpdateTimes = 0.0f;
int NumUpdateTimes = 0;
float AverageUpdateTime = -1.0f; // Invalid value at first
bool advanceInfo = false;
int tankCam = 0;
bool camset = false;
CEntity* target1;
CEntity* target2;
CMatrix4x4 camPos;
float ammoTimer = 5.0f;
bool AmmoSpawn = false;
int ammoCount = 0;

//-----------------------------------------------------------------------------
// Scene management
//-----------------------------------------------------------------------------

// Creates the scene geometry
bool SceneSetup()
{
	//////////////////////////////////////////////
	// Prepare render methods

	InitialiseMethods();

	//LOAD XML file templates
	LevelParser.ParseFile("Entities.xml");

	/////////////////////////////
	// Camera / light setup

	// Set camera position and clip planes
	MainCamera = new CCamera(CVector3(0.0f, 30.0f, -100.0f), CVector3(ToRadians(15.0f), 0, 0));
	MainCamera->SetNearFarClip(1.0f, 20000.0f);

	// Sunlight and light in building
	Lights[0] = new CLight(CVector3(-5000.0f, 4000.0f, -10000.0f), SColourRGBA(1.0f, 0.9f, 0.6f), 15000.0f);
	Lights[1] = new CLight(CVector3(6.0f, 7.5f, 40.0f), SColourRGBA(1.0f, 0.0f, 0.0f), 1.0f);

	// Ambient light level
	AmbientLight = SColourRGBA(0.6f, 0.6f, 0.6f, 1.0f);

	target1 = EntityManager.GetEntity("R-1");
	target2 = EntityManager.GetEntity("O-1");
	return true;
}


// Release everything in the scene
void SceneShutdown()
{
	// Release render methods
	ReleaseMethods();

	// Release lights
	for (int light = NumLights - 1; light >= 0; --light)
	{
		delete Lights[light];
	}

	// Release camera
	delete MainCamera;

	// Destroy all entities
	EntityManager.DestroyAllEntities();
	EntityManager.DestroyAllTemplates();
}


//-----------------------------------------------------------------------------
// Game Helper functions
//-----------------------------------------------------------------------------

// Get UID of tank A (team 0) or B (team 1)
TEntityUID GetTankUID(int team)
{
	return (team == 0) ? Tanks[0] : Tanks[1];
}


//-----------------------------------------------------------------------------
// Game loop functions
//-----------------------------------------------------------------------------

// Draw one frame of the scene
void RenderScene( float updateTime )
{
	// Setup the viewport - defines which part of the back-buffer we will render to (usually all of it)
	D3D10_VIEWPORT vp;
	vp.Width  = ViewportWidth;
	vp.Height = ViewportHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pd3dDevice->RSSetViewports( 1, &vp );

	// Select the back buffer and depth buffer to use for rendering
	g_pd3dDevice->OMSetRenderTargets( 1, &BackBufferRenderTarget, DepthStencilView );
	
	// Clear previous frame from back buffer and depth buffer
	g_pd3dDevice->ClearRenderTargetView( BackBufferRenderTarget, &AmbientLight.r );
	g_pd3dDevice->ClearDepthStencilView( DepthStencilView, D3D10_CLEAR_DEPTH, 1.0f, 0 );

	// Update camera aspect ratio based on viewport size - for better results when changing window size
	MainCamera->SetAspect( static_cast<TFloat32>(ViewportWidth) / ViewportHeight );

	// Set camera and light data in shaders
	MainCamera->CalculateMatrices();
	SetCamera(MainCamera);
	SetAmbientLight(AmbientLight);
	SetLights(&Lights[0]);

	// Render entities and draw on-screen text
	EntityManager.RenderAllEntities();
	RenderSceneText( updateTime );

    // Present the backbuffer contents to the display
	SwapChain->Present( 0, 0 );
}


// Render a single text string at the given position in the given colour, may optionally centre it
void RenderText( const string& text, int X, int Y, float r, float g, float b, bool centre = false )
{
	RECT rect;
	if (!centre)
	{
		SetRect( &rect, X, Y, 0, 0 );
		OSDFont->DrawText( NULL, text.c_str(), -1, &rect, DT_NOCLIP, D3DXCOLOR( r, g, b, 1.0f ) );
	}
	else
	{
		SetRect( &rect, X - 100, Y, X + 100, 0 );
		OSDFont->DrawText( NULL, text.c_str(), -1, &rect, DT_CENTER | DT_NOCLIP, D3DXCOLOR( r, g, b, 1.0f ) );
	}


}

// Render on-screen text each frame
void RenderSceneText( float updateTime )
{
	// Accumulate update times to calculate the average over a given period
	SumUpdateTimes += updateTime;
	++NumUpdateTimes;
	if (SumUpdateTimes >= UpdateTimePeriod)
	{
		AverageUpdateTime = SumUpdateTimes / NumUpdateTimes;
		SumUpdateTimes = 0.0f;
		NumUpdateTimes = 0;
	}

	// Write FPS text string
	stringstream outText;
	if (AverageUpdateTime >= 0.0f)
	{
		outText << "Frame Time: " << AverageUpdateTime * 1000.0f << "ms" << endl << "FPS:" << 1.0f / AverageUpdateTime;
		RenderText( outText.str(), 2, 2, 0.0f, 0.0f, 0.0f );
		RenderText( outText.str(), 0, 0, 1.0f, 1.0f, 0.0f );
		outText.str("");
	}

	EntityManager.BeginEnumEntities("", "", "Tank");
	CEntity* tankEntity = EntityManager.EnumEntity();

	while (tankEntity != 0)
	{
		CEntity* entity = tankEntity;
		if (entity)
		{
			CTankEntity* tankEntity = static_cast<CTankEntity*>(entity);
			CVector3 TankPt = tankEntity->Position();

			CVector3 rgb = { 0.0f, 0.0f, 0.0f };
			// Convert monster world position to pixel coordinate (picking in Camera class)
			int X, Y;
			if (MainCamera->PixelFromWorldPt(TankPt, ViewportWidth, ViewportHeight, &X, &Y))
			{
				//Sets colour to diferent team texts.
				if (tankEntity->getTeam() == 0) { rgb = { 0.0f, 1.0f, 0.0f }; }
				else if (tankEntity->getTeam() == 1) { rgb = { 1.0f, 0.0f, 0.0f }; }

				if (advanceInfo == false)
				{
					outText << tankEntity->Template()->GetName().c_str() << ": "
						<< tankEntity->GetName().c_str();
				}
				else
				{
					//Prints all tank info on the tank.
					outText << tankEntity->Template()->GetName().c_str() << ": "
						<< tankEntity->GetName().c_str() << endl << "HP: " << tankEntity->getHP() << " State: " << tankEntity->getState()
						<< endl << "Fired: " << tankEntity->getFired() << " Ammo: " << tankEntity->getAmmo();
				}

				RenderText(outText.str(), X, Y, rgb.x, rgb.y, rgb.z, true);

				outText.str("");
			}
		}

		tankEntity = EntityManager.EnumEntity();
	}
	outText.str("");
}


// Update the scene between rendering
void UpdateScene( float updateTime )
{
	// Call all entity update functions
	EntityManager.UpdateAllEntities( updateTime );

	// Set camera speeds
	// Key F1 used for full screen toggle
	if (KeyHit(Key_F2)) CameraMoveSpeed = 5.0f;
	if (KeyHit(Key_F3)) CameraMoveSpeed = 40.0f;

	//Start Game key
	if (KeyHit(Key_1))
	{
		EntityManager.BeginEnumEntities("", "", "Tank");
		CEntity* tankEntityStart = EntityManager.EnumEntity();

		while (tankEntityStart != 0)
		{
			if (tankEntityStart)
			{
				SMessage start;
				start.from = SystemUID;
				start.type = Msg_Start;
				Messenger.SendMessage(tankEntityStart->GetUID(), start);
			}
			tankEntityStart = EntityManager.EnumEntity();
		}
		AmmoSpawn = true; //Allows ammo to start spawning
	}

	//Sends stop message.
	if (KeyHit(Key_2))
	{
		EntityManager.BeginEnumEntities("", "", "Tank");
		CEntity* tankEntityStop = EntityManager.EnumEntity();

		while (tankEntityStop != 0)
		{
			if (tankEntityStop)
			{
				SMessage stop;
				stop.from = SystemUID;
				stop.type = Msg_Stop;
				Messenger.SendMessage(tankEntityStop->GetUID(), stop);
			}
			tankEntityStop = EntityManager.EnumEntity();
		}
		AmmoSpawn = false; //Allows ammo to start spawning
	}

	if (KeyHit(Key_0)) { advanceInfo = !advanceInfo; }
	// Move the camera

	//Chase cam controls
	if (KeyHit(Key_Numpad1)) { tankCam = 0; }
	if (KeyHit(Key_Numpad2)) { tankCam = 1; }
	if (KeyHit(Key_Numpad3)) { tankCam = 2; }

	//Chase camera settings.
	if (tankCam == 1 && target1->GetName() == "R-1")
	{
		camPos = target1->Matrix();
		camPos.Position().y += 4.0f;
		camPos.MoveLocalZ(-15);
		MainCamera->Matrix() = camPos;
	}
	else if (tankCam == 2 && target2->GetName() == "O-1")
	{
		camPos = target2->Matrix();
		camPos.Position().y += 4.0f;
		camPos.MoveLocalZ(-15);
		MainCamera->Matrix() = camPos;
	}
	else
	{
		MainCamera->Control(Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D,
			CameraMoveSpeed * updateTime, CameraRotSpeed * updateTime);
	}

	//Ammo crate spawning.
	if (AmmoSpawn == true)
	{
		ammoCount = 0;
		EntityManager.BeginEnumEntities("", "", "Ammo");
		CEntity* AmmoEntity = EntityManager.EnumEntity();
		if (AmmoEntity != NULL)
		{
			if (AmmoEntity != 0)
			{
				ammoCount++;
			}
		}
		if (ammoCount < 1)
		{
			ammoTimer -= updateTime;
			if (ammoTimer <= 0.0f)
			{
				CVector3 randpos = { Random(-100.0f, 100.0f), 20.0f, Random(-100.0f, 100.0f) };
				CVector3 rot = { 0.0f, 0.0f, 0.0f };
				EntityManager.CreateAmmo("AmmoCrate", "Ammo", randpos, rot , CVector3{ 0.2f, 0.2f, 0.1f });
				ammoTimer = 5.0f;
			}
		}
	}
}


} // namespace gen

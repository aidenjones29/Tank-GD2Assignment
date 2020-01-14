/*******************************************
	ShellEntity.cpp

	Shell entity class
********************************************/

#include "ShellEntity.h"
#include "TankEntity.h"
#include "EntityManager.h"
#include "Messenger.h"

namespace gen
{

// Reference to entity manager from TankAssignment.cpp, allows look up of entities by name, UID etc.
// Can then access other entity's data. See the CEntityManager.h file for functions. Example:
//    CVector3 targetPos = EntityManager.GetEntity( targetUID )->GetMatrix().Position();
extern CEntityManager EntityManager;

// Messenger class for sending messages to and between entities
extern CMessenger Messenger;

// Helper function made available from TankAssignment.cpp - gets UID of tank A (team 0) or B (team 1).
// Will be needed to implement the required shell behaviour in the Update function below
extern TEntityUID GetTankUID( int team );

extern float pointToPoint(CVector3 pt1, CVector3 pt2);

/*-----------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------
	Shell Entity Class
-------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------*/

// Shell constructor intialises shell-specific data and passes its parameters to the base
// class constructor
CShellEntity::CShellEntity
(
	CEntityTemplate* entityTemplate,
	TEntityUID       UID,
	const string&    name /*=""*/,
	const CVector3&  position /*= CVector3::kOrigin*/, 
	const CVector3&  rotation /*= CVector3( 0.0f, 0.0f, 0.0f )*/,
	const CVector3&  scale /*= CVector3( 1.0f, 1.0f, 1.0f )*/
) : CEntity( entityTemplate, UID, name, position, rotation, scale )
{
	timer = 3.0f;
}


// Update the shell - controls its behaviour. The shell code is empty, it needs to be written as
// one of the assignment requirements
// Return false if the entity is to be destroyed
bool CShellEntity::Update( TFloat32 updateTime )
{
	if (timer >= 0.0f)
	{
		Matrix().MoveLocalZ(0.5f);
		timer -= updateTime;

		EntityManager.BeginEnumEntities("", "", "Tank");
		CEntity* tankEntity = EntityManager.EnumEntity();
		CTankEntity* tank = static_cast<CTankEntity*>(tankEntity);
		while (tankEntity != 0)
		{
			if (GetName() != tankEntity->GetName() && tank->getState() != "Dead")
			{
				float dist = pointToPoint(Matrix().Position(), tankEntity->Position());
				if (dist <= 5.0f)
				{
					SMessage Hit;
					Hit.from = SystemUID;
					Hit.type = Msg_Hit;
					Messenger.SendMessage(tankEntity->GetUID(), Hit);

					return false;
				}
			}

			tankEntity = EntityManager.EnumEntity();
		}

		return true;
	}
	else
	{
		return false;
	}
}

//float pointToPoint(CVector3 pt1, CVector3 pt2)
//{
//	CVector3 v = pt2 - pt1;
//	return sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
//}

} // namespace gen

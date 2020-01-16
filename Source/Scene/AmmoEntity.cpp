#include "ShellEntity.h"
#include "TankEntity.h"
#include "AmmoEntity.h"
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
	extern TEntityUID GetTankUID(int team);

	extern float pointToPoint(CVector3 pt1, CVector3 pt2);

	/*-----------------------------------------------------------------------------------------
	-------------------------------------------------------------------------------------------
		Shell Entity Class
	-------------------------------------------------------------------------------------------
	-----------------------------------------------------------------------------------------*/

	// Shell constructor intialises shell-specific data and passes its parameters to the base
	// class constructor
	CAmmoEntity::CAmmoEntity
	(
		CEntityTemplate* entityTemplate,
		TEntityUID       UID,
		const string& name /*=""*/,
		const CVector3& position /*= CVector3::kOrigin*/,
		const CVector3& rotation /*= CVector3( 0.0f, 0.0f, 0.0f )*/,
		const CVector3& scale /*= CVector3( 1.0f, 1.0f, 1.0f )*/
	) : CEntity(entityTemplate, UID, name, position, rotation, scale)
	{
		timer = 3.0f;
	}


	// Update the shell - controls its behaviour. The shell code is empty, it needs to be written as
	// one of the assignment requirements
	// Return false if the entity is to be destroyed
	bool CAmmoEntity::Update(TFloat32 updateTime)
	{
		Matrix().RotateLocalY(1.0f * updateTime);

		EntityManager.BeginEnumEntities("", "", "Tank");
		CEntity* tankEntity = EntityManager.EnumEntity();
		while (tankEntity != 0)
		{
		CTankEntity* tank = static_cast<CTankEntity*>(tankEntity);
			if (tank->getState() == "Ammo")
			{
				float dist = pointToPoint(Matrix().Position(), tankEntity->Position());
				if (dist <= 10.0f)
				{
					SMessage Ammo;
					Ammo.from = SystemUID;
					Ammo.type = Msg_Ammo;
					Messenger.SendMessage(tankEntity->GetUID(), Ammo);

					return false;
				}
			}

			tankEntity = EntityManager.EnumEntity();
		}

		return true;
			return true;
	}
} // namespace gen

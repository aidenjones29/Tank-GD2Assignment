/*******************************************
	TankEntity.cpp

	Tank entity template and entity classes
********************************************/

// Additional technical notes for the assignment:
// - Each tank has a team number (0 or 1), HP and other instance data - see the end of TankEntity.h
//   You will need to add other instance data suitable for the assignment requirements

// - A function GetTankUID is defined in TankAssignment.cpp and made available here, which returns
//   the UID of the tank on a given team. This can be used to get the enemy tank UID

// - Tanks have three parts: the root, the body and the turret. Each part has its own matrix, which
//   can be accessed with the Matrix function - root: Matrix(), body: Matrix(1), turret: Matrix(2)
//   However, the body and turret matrix are relative to the root's matrix - so to get the actual 
//   world matrix of the body, for example, we must multiply: Matrix(1) * Matrix()

// - Vector facing work similar to the car tag lab will be needed for the turret->enemy facing 
//   requirements for the Patrol and Aim states
// - The CMatrix4x4 function DecomposeAffineEuler allows you to extract the x,y & z rotations
//   of a matrix. This can be used on the *relative* turret matrix to help in rotating it to face
//   forwards in Evade state
// - The CShellEntity class is simply an outline. To support shell firing, you will need to add
//   member data to it and rewrite its constructor & update function. You will also need to update 
//   the CreateShell function in EntityManager.cpp to pass any additional constructor data required
// - Destroy an entity by returning false from its Update function - the entity manager wil perform
//   the destruction. Don't try to call DestroyEntity from within the Update function.
// - As entities can be destroyed, you must check that entity UIDs refer to existant entities, before
//   using their entity pointers. The return value from EntityManager.GetEntity will be NULL if the
//   entity no longer exists. Use this to avoid trying to target a tank that no longer exists etc.

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
	// Will be needed to implement the required tank behaviour in the Update function below
	extern TEntityUID GetTankUID(int team);

	float pointToPoint(CVector3 pt1, CVector3 pt2);

	/*-----------------------------------------------------------------------------------------
	-------------------------------------------------------------------------------------------
		Tank Entity Class
	-------------------------------------------------------------------------------------------
	-----------------------------------------------------------------------------------------*/

	// Tank constructor intialises tank-specific data and passes its parameters to the base
	// class constructor
	CTankEntity::CTankEntity
	(
		CTankTemplate* tankTemplate,
		TEntityUID      UID,
		TUInt32         team,
		const string& name /*=""*/,
		const CVector3& position /*= CVector3::kOrigin*/,
		const CVector3& rotation /*= CVector3( 0.0f, 0.0f, 0.0f )*/,
		const CVector3& scale /*= CVector3( 1.0f, 1.0f, 1.0f )*/
	) : CEntity(tankTemplate, UID, name, position, rotation, scale)
	{
		m_TankTemplate = tankTemplate;
		// Tanks are on teams so they know who the enemy is
		m_Team = team;
		// Initialise other tank data and state
		m_Speed = 0.0f;
		m_HP = m_TankTemplate->GetMaxHP();
		m_State = Inactive;
		m_Timer = 1.0f;
		m_patrolPoint[0] = position;
		m_patrolPoint[1] = position;
		m_patrolPoint[0].z += Random(0, 30);
		m_patrolPoint[1].z -= Random(0, 30);
		m_patrolPoint[0].x += Random(0, 30);
		m_patrolPoint[1].x -= Random(0, 30);
		m_currentPoint = 1;
		m_Fired = 0;
		m_Facing = false;
		m_deadSpeed = 0.1f;
		m_ammoCount = 2;
	}


	// Update the tank - controls its behaviour. The shell code just performs some test behaviour, it
	// is to be rewritten as one of the assignment requirements
	// Return false if the entity is to be destroyed
	bool CTankEntity::Update(TFloat32 updateTime)
	{
		//CEntity* tankE = EntityManager.GetEntity(GetUID());
		// Fetch any messages
		SMessage msg;
		while (Messenger.FetchMessage(GetUID(), &msg))
		{
			// Set state variables based on received messages
			switch (msg.type)
			{
			case Msg_Start:
				m_State = Patrol;
				break;
			case Msg_Stop:
				m_State = Inactive;
				break;
			case Msg_Hit:
				m_HP -= 20;
				break;
			case Msg_Ammo:
				m_ammoCount = 10;
				m_State = Patrol;
				break;
			}
		}

		if (m_HP <= 0)
		{
			m_State = Dead;
		}

		// Tank behaviour
		// Only move if in Go state
		if (m_State == Patrol)
		{
			CVector3 tankPos = Matrix().GetPosition();

			float distance = pointToPoint(tankPos, m_patrolPoint[m_currentPoint]);
			CVector3 tankFacing = Matrix().ZAxis();
			CVector3 turnVec = tankPos - m_patrolPoint[m_currentPoint];
			
			if (Dot(Normalise(tankFacing), Normalise(turnVec)) < Cos(2.0f * updateTime))
			{
				CVector3 tankXVec = Matrix().XAxis();
				if (Dot(tankXVec, turnVec) >= -0.5f && Dot(tankXVec, turnVec) <= 0.5f)
				{
					m_Facing = true;
				}
			
				if (m_Facing == false)
				{
					if (Dot(tankXVec, turnVec) < 0)
					{
						Matrix().RotateLocalY(m_TankTemplate->GetTurnSpeed() * updateTime);
					}
					else if (Dot(tankXVec, turnVec) > 0)
					{
						Matrix().RotateLocalY(-m_TankTemplate->GetTurnSpeed() * updateTime);
					}
				}
			
				//m_Speed = 20.0f;
				Matrix().MoveLocalZ(m_TankTemplate->GetMaxSpeed() * updateTime);
			}
			else if (distance <= 10.0f)
			{
				if (m_currentPoint == 1)
				{
					m_currentPoint = 0;
					m_Facing = false;
				}
				else if (m_currentPoint == 0)
				{
					m_currentPoint = 1;
					m_Facing = false;
				}
			}
			else
			{
				Matrix().FaceTarget(turnVec);
			}
			
			
			
			Matrix(2).RotateLocalY(m_TankTemplate->GetTurretTurnSpeed() * updateTime);
			
			CMatrix4x4 turretWorld = Matrix(2) * Matrix();
			
			CVector3 turPos;
			CVector3 turRot;
			CVector3 turScale;
			
			turretWorld.DecomposeAffineEuler(&turPos, &turRot, &turScale);
			
			EntityManager.BeginEnumEntities("", "", "Tank");
			CEntity* tankEntity = EntityManager.EnumEntity();
			
			while (tankEntity != 0)
			{
				CTankEntity* tank = static_cast<CTankEntity*>(tankEntity);
				if (tankEntity->GetUID() != GetUID() && tank->m_Team != m_Team)
				{
					float length1 = LengthSquared(turretWorld.ZAxis());
					CVector3 turTurnVec = tankEntity->Position() - turretWorld.Position();
					float length2 = LengthSquared(turTurnVec);
					float dotAngle = Dot(turretWorld.ZAxis(), turTurnVec);
			
					float angle = dotAngle / ((Sqrt(length2) * Sqrt(length1)));
					float angle2 = acos(angle) * 180 / 3.14159265359;
			
					if (angle2 < 5.0f)
					{
						m_Target = tankEntity->GetUID();
						m_State = Aim;
						m_Facing = false;
						m_Timer = 1.0f;
					}
				}
			
				tankEntity = EntityManager.EnumEntity();
			}
		}
		else if (m_State == Aim)
		{

			if (m_Timer >= 0.0f && m_ammoCount >= 0)
			{
				if (EntityManager.GetEntity(m_Target))
				{
					CMatrix4x4 turretWorld = Matrix(2) * Matrix();
					CEntity* tankTarget = EntityManager.GetEntity(m_Target);
					CVector3 turTurnVec = tankTarget->Position() - turretWorld.Position();
					Normalise(turTurnVec);
			
					if (Dot(turretWorld.XAxis(), turTurnVec) > 0.0f)
					{
						Matrix(2).RotateLocalY(m_TankTemplate->GetTurretTurnSpeed() * updateTime);
					}
					else if (Dot(turretWorld.XAxis(), turTurnVec) < -0.0f)
					{
						Matrix(2).RotateLocalY(-m_TankTemplate->GetTurretTurnSpeed() * updateTime);
					}
			
					m_Timer -= updateTime;
				}
				else
				{
					m_State = Patrol;
				}
			}
			else
			{
				if (m_ammoCount >= 0)
				{
					CMatrix4x4 turretWorld = Matrix(2) * Matrix();
					CVector3 turPos;
					CVector3 turRot;
					CVector3 turScale;
					turretWorld.DecomposeAffineEuler(&turPos, &turRot, NULL);
					EntityManager.CreateShell("Shell", GetName(), turretWorld.Position(), turRot);
					m_Fired++;
					m_ammoCount--;
				}
			
				CVector3 curPos = Matrix().Position();
				curPos.x += Random(-40.0f, 40.0f);
				curPos.z += Random(-40.0f, 40.0f);
				m_EvadePoint = curPos;
				if (m_ammoCount >= 0) { m_State = Evade; }
				else { m_State = Ammo; }
			}
		}
		else if (m_State == Evade)
		{	
			CVector3 tankPos = Matrix().GetPosition();

			float distance = pointToPoint(tankPos, m_EvadePoint);

			if (distance >= 10.0f)
			{
				CVector3 turnVec = tankPos - m_EvadePoint;
				CVector3 tankFacing = Matrix().ZAxis();
				CVector3 tankXVec = Matrix().XAxis();

				if (Dot(tankXVec, turnVec) >= -0.5f && Dot(tankXVec, turnVec) <= 0.5f)
				{
					m_Facing = true;
				}

				if (m_Facing == false)
				{
					if (Dot(tankXVec, turnVec) < 0)
					{
						Matrix().RotateLocalY(m_TankTemplate->GetTurnSpeed() * updateTime);
					}
					else if (Dot(tankXVec, turnVec) > 0)
					{
						Matrix().RotateLocalY(-m_TankTemplate->GetTurnSpeed() * updateTime);
					}
				}
				Matrix().MoveLocalZ(m_TankTemplate->GetMaxSpeed() * updateTime);
			}
			else
			{
				if (m_ammoCount <= 0)
				{
					m_State = Ammo;
					m_Facing = false;
				}
				else
				{
					m_State = Patrol;
					m_Facing = false;
				}
			}
		}
		else if (m_State == Dead)
		{
			Matrix(2).RotateY(1.0f);
			Matrix(2).MoveLocalY(m_deadSpeed);
			Matrix().MoveLocalY(-m_deadSpeed);
			if (Matrix(2).Position().y >= 6)
			{
				m_deadSpeed = -0.1f;
			}
			if (Matrix(2).Position().y <= 0)
			{
				return false;
			}

		}
		else if (m_State == Ammo)
		{

			EntityManager.BeginEnumEntities("", "", "Ammo");
			CEntity* AmmoEntity = EntityManager.EnumEntity();
			if (AmmoEntity != NULL)
			{
				if (AmmoEntity != 0)
				{
					m_EvadePoint = AmmoEntity->Matrix().Position();
					//AmmoEntity = EntityManager.EnumEntity();
				}

					CVector3 tankPos = Matrix().GetPosition();
					float distance = pointToPoint(tankPos, m_EvadePoint);
					if (distance >= 5.0f)
					{
						CVector3 turnVec = tankPos - m_EvadePoint;
						CVector3 tankFacing = Matrix().ZAxis();
						CVector3 tankXVec = Matrix().XAxis();

						if (m_Facing == false)
						{
							if (Dot(tankXVec, turnVec) < 0)
							{
								Matrix().RotateLocalY(m_TankTemplate->GetTurnSpeed() * updateTime);
							}
							else if (Dot(tankXVec, turnVec) > 0)
							{
								Matrix().RotateLocalY(-m_TankTemplate->GetTurnSpeed() * updateTime);
							}
						}

						Matrix().MoveLocalZ((m_TankTemplate->GetMaxSpeed() * updateTime));
					}
					Matrix().SetY(0.5f);
			}
			else
			{
				CVector3 curPos = Matrix().Position();
				curPos.x += Random(-40.0f, 40.0f);
				curPos.z += Random(-40.0f, 40.0f);
				m_EvadePoint = curPos;
				m_State = Evade;
			}
		}
		else
		{
			m_Speed = 0;
		}

		// Perform movement...
		// Move along local Z axis scaled by update time

		return true; // Don't destroy the entity
	}

	float pointToPoint(CVector3 pt1, CVector3 pt2)
	{
		CVector3 v = pt2 - pt1;
		return sqrt((v.x * v.x) + (v.z * v.z));
	}


} // namespace gen

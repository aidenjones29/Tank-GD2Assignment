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
		m_Speed = 0.0f; //Current movement speed of the tanks.
		m_HP = m_TankTemplate->GetMaxHP(); 
		m_State = Inactive; //Starting state.
		m_Timer = 1.0f; //Timer used for aiming time

		//Initial patrol points set from each origin.
		m_patrolPoint[0] = position;
		m_patrolPoint[1] = position;
		m_patrolPoint[0].z += Random(0, 30);
		m_patrolPoint[1].z -= Random(0, 30);
		m_patrolPoint[0].x += Random(0, 30);
		m_patrolPoint[1].x -= Random(0, 30);

		m_currentPoint = 0; //Starting patrol point
		m_Fired = 0; //Used for keeping track of shells fired.
		m_Facing = false; //Am i facing the target? (Stops jittering)
		m_deadSpeed = 0.1f; //Speed of dead "animation"
		m_ammoCount = 5; //Max ammo.
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
				m_ammoCount = 5;
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
			Matrix().SetY(0.5f);
			float distance = pointToPoint(tankPos, m_patrolPoint[m_currentPoint]);
			CVector3 tankFacing = Matrix().ZAxis();
			CVector3 turnVec = tankPos - m_patrolPoint[m_currentPoint];
			
			//Turns to face the target point.
			if (Dot(Normalise(tankFacing), Normalise(turnVec)) < Cos(2.0f * updateTime))
			{
				CVector3 tankXVec = Matrix().XAxis();
				if (Dot(tankXVec, turnVec) >= -0.5f && Dot(tankXVec, turnVec) <= 0.5f)
				{   //Checks if facing to stop shaking.
					m_Facing = true;
				}
			
				if (m_Facing == false)
				{
					//is the target left or right.
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
				if (m_currentPoint == 1) //Changes patrol point.
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
			
			
			//Turret rotation and enemy checking.
			Matrix(2).RotateLocalY(m_TankTemplate->GetTurretTurnSpeed() * updateTime);
			
			CMatrix4x4 turretWorld = Matrix(2) * Matrix();
			
			CVector3 turPos;
			CVector3 turRot;
			CVector3 turScale;
			
			turretWorld.DecomposeAffineEuler(&turPos, &turRot, &turScale);
			
			EntityManager.BeginEnumEntities("", "", "Tank");
			CEntity* tankEntity = EntityManager.EnumEntity();
			
			//Checks all tanks for being within a cone of the turret.
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
			//Follows the target tank and fires.
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
					//Creation of a shell with the turrets orientation.
					CMatrix4x4 turretWorld = Matrix(2) * Matrix();
					CVector3 turPos;
					CVector3 turRot;
					CVector3 turScale;
					turretWorld.DecomposeAffineEuler(&turPos, &turRot, NULL);
					EntityManager.CreateShell("Shell", GetName(), turretWorld.Position(), turRot);
					m_Fired++;
					m_ammoCount--;
				}
				
				//Sets an evade point at random for evade state.
				CVector3 curPos = Matrix().Position();
				curPos.x += Random(-40.0f, 40.0f);
				curPos.z += Random(-40.0f, 40.0f);
				m_EvadePoint = curPos;
				//If the tank has no ammo sets to ammo state.
				if (m_ammoCount >= 0) { m_State = Evade; }
				else { m_State = Ammo; }
			}
		}
		else if (m_State == Evade)
		{	
			CVector3 tankPos = Matrix().GetPosition();
			Matrix().SetY(0.5f);
			float distance = pointToPoint(tankPos, m_EvadePoint);

			//If target point is far away face it.
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

				//Rotates the turrt to the facing vector of the tank to face forward.
				CMatrix4x4 turretWorld = Matrix(2) * Matrix();
				CEntity* tankTarget = EntityManager.GetEntity(GetUID());
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
			}
			else
			{
				//Loops ammo and evade state if no ammo found.
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
			//Preforms the death animation.
			Matrix(2).RotateY(1.0f);
			Matrix(2).MoveLocalY(m_deadSpeed);
			if (Matrix(2).Position().y >= 6)
			{
				m_deadSpeed = -0.1f;
			}
			if (Matrix(2).Position().y <= 0)
			{
				return false; //Destorys the tank.
			}

		}
		else if (m_State == Ammo)
		{
			//Find ammo crate if one spawned.
			EntityManager.BeginEnumEntities("", "", "Ammo");
			CEntity* AmmoEntity = EntityManager.EnumEntity();
			if (AmmoEntity != NULL)
			{
				//Uses evade point to move for ammo.
				if (AmmoEntity != 0)
				{
					m_EvadePoint = AmmoEntity->Matrix().Position();
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
				//If no ammo available go back to evade with a new random pos
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

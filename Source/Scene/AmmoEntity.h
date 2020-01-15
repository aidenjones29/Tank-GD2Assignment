#pragma once

#include <string>
using namespace std;

#include "Defines.h"
#include "CVector3.h"
#include "Entity.h"

namespace gen
{

	class CAmmoEntity : public CEntity
	{
		/////////////////////////////////////
		//	Constructors/Destructors
	public:
		// Shell constructor intialises shell-specific data and passes its parameters to the base
		// class constructor
		CAmmoEntity
		(
			CEntityTemplate* entityTemplate,
			TEntityUID       UID,
			const string& name = "",
			const CVector3& position = CVector3::kOrigin,
			const CVector3& rotation = CVector3(0.0f, 0.0f, 0.0f),
			const CVector3& scale = CVector3(1.0f, 1.0f, 1.0f)
		);

		// No destructor needed


	/////////////////////////////////////
	//	Public interface
	public:

		/////////////////////////////////////
		// Update

		// Update the shell - performs simple shell behaviour
		// Return false if the entity is to be destroyed
		// Keep as a virtual function in case of further derivation
		virtual bool Update(TFloat32 updateTime);


		/////////////////////////////////////
		//	Private interface
	private:

		/////////////////////////////////////
		// Data
		float timer;
		// Add your shell data here
	};


} // namespace gen

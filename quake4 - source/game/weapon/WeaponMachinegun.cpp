#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "../Weapon.h"
#include "../Projectile.h" //Added to allow guided projectiles

#define BLASTER_SPARM_CHARGEGLOW		6

class rvWeaponMachinegun : public rvWeapon {
public:

	CLASS_PROTOTYPE(rvWeaponMachinegun);

	rvWeaponMachinegun(void);
	~rvWeaponMachinegun(void);

	virtual void		Spawn(void);
	virtual void		Think(void); //Added to allow guided projectiles
	void				Save(idSaveGame *savefile) const;
	void				Restore(idRestoreGame *savefile);
	void				PreSave(void);
	void				PostSave(void);

protected:

	bool				UpdateAttack(void);
	bool				UpdateFlashlight(void);
	void				Flashlight(bool on);
	//Dembner Begin
	idEntityPtr<idEntity>				guideEnt;
	int									guideTime;
	int									guideStartTime;
	rvClientEntityPtr<rvClientEffect>	guideEffect;
	bool								guideLocked;
	float								guideRange;
	int									guideHoldTime;
	int									guideAquireTime;

	bool								isCharged;

	jointHandle_t						jointDrumView;
	jointHandle_t						jointPinsView;
	jointHandle_t						jointSteamRightView;
	jointHandle_t						jointSteamLeftView;

	jointHandle_t						jointGuideEnt;

	virtual void		OnLaunchProjectile(idProjectile* proj);
	//Dembner End
private:

	//Dembner Begin
	void				UpdateGuideStatus(float range = 0.0f);
	void				CancelGuide(void);
	//Dembner End

	int					chargeTime;
	int					chargeDelay;
	idVec2				chargeGlow;
	bool				fireForced;
	int					fireHeldTime;

	stateResult_t		State_Raise(const stateParms_t& parms);
	stateResult_t		State_Lower(const stateParms_t& parms);
	stateResult_t		State_Idle(const stateParms_t& parms);
	stateResult_t		State_Charge(const stateParms_t& parms);
	stateResult_t		State_Charged(const stateParms_t& parms);
	stateResult_t		State_Fire(const stateParms_t& parms);
	stateResult_t		State_Flashlight(const stateParms_t& parms);

	CLASS_STATES_PROTOTYPE(rvWeaponMachinegun);
};

CLASS_DECLARATION(rvWeapon, rvWeaponMachinegun)
END_CLASS

/*
================
rvWeaponMachinegun::rvWeaponMachinegun
================
*/
rvWeaponMachinegun::rvWeaponMachinegun(void) {
}

/*
================
rvWeaponMachinegun::~rvWeaponMachinegungun
================
*/
rvWeaponMachinegun::~rvWeaponMachinegun(void) {
	if (guideEffect) {
		guideEffect->Stop();
	}
}

/*
================
rvWeaponMachinegun::UpdateFlashlight
================
*/
bool rvWeaponMachinegun::UpdateFlashlight(void) {
	if (!wsfl.flashlight) {
		return false;
	}

	SetState("Flashlight", 0);
	return true;
}

/*
================
rvWeaponMachinegun::Flashlight
================
*/
void rvWeaponMachinegun::Flashlight(bool on) {
	owner->Flashlight(on);

	if (on) {
		worldModel->ShowSurface("models/weapons/blaster/flare");
		viewModel->ShowSurface("models/weapons/blaster/flare");
	}
	else {
		worldModel->HideSurface("models/weapons/blaster/flare");
		viewModel->HideSurface("models/weapons/blaster/flare");
	}
}

/*
================
rvWeaponMachinegun::UpdateAttack
================
*/
bool rvWeaponMachinegun::UpdateAttack(void) {
	// Clear fire forced
	if (fireForced) {
		if (!wsfl.attack) {
			fireForced = false;
		}
		else {
			return false;
		}
	}

	// If the player is pressing the fire button and they have enough ammo for a shot
	// then start the shooting process.
	if (wsfl.attack && gameLocal.time >= nextAttackTime) {
		// Save the time which the fire button was pressed
		if (fireHeldTime == 0) {
			nextAttackTime = gameLocal.time + (fireRate * owner->PowerUpModifier(PMOD_FIRERATE));
			fireHeldTime = gameLocal.time;
			viewModel->SetShaderParm(BLASTER_SPARM_CHARGEGLOW, chargeGlow[0]);
		}
	}

	// If they have the charge mod and they have overcome the initial charge 
	// delay then transition to the charge state.
	if (fireHeldTime != 0) {
		if (gameLocal.time - fireHeldTime > chargeDelay) {
			SetState("Charge", 4);
			return true;
		}

		// If the fire button was let go but was pressed at one point then 
		// release the shot.
		if (!wsfl.attack) {
			idPlayer * player = gameLocal.GetLocalPlayer();
			if (player) {

				if (player->GuiActive()) {
					//make sure the player isn't looking at a gui first
					SetState("Lower", 0);
				}
				else {
					SetState("Fire", 0);
				}
			}
			return true;
		}
	}

	return false;
}

/*
================
rvWeaponMachinegun::Spawn
================
*/
void rvWeaponMachinegun::Spawn(void) {

	spawnArgs.GetFloat("lockRange", "1000", guideRange);
	guideHoldTime = SEC2MS(spawnArgs.GetFloat("lockHoldTime", "10"));
	guideAquireTime = SEC2MS(spawnArgs.GetFloat("lockAquireTime", ".1"));

	viewModel->SetShaderParm(BLASTER_SPARM_CHARGEGLOW, 0);
	SetState("Raise", 0);

	chargeGlow = spawnArgs.GetVec2("chargeGlow");
	chargeTime = SEC2MS(spawnArgs.GetFloat("chargeTime"));
	chargeDelay = SEC2MS(spawnArgs.GetFloat("chargeDelay"));

	fireHeldTime = 0;
	fireForced = false;

	Flashlight(owner->IsFlashlightOn());
}

/*
================
rvWeaponMachinegun::Save
================
*/
void rvWeaponMachinegun::Save(idSaveGame *savefile) const {
	savefile->WriteInt(chargeTime);
	savefile->WriteInt(chargeDelay);
	savefile->WriteVec2(chargeGlow);
	savefile->WriteBool(fireForced);
	savefile->WriteInt(fireHeldTime);
}

/*
================
rvWeaponMachinegun::CancelGuide
================
*/
void rvWeaponMachinegun::CancelGuide(void) {
	if (zoomGui && guideEnt) {
		zoomGui->HandleNamedEvent("lockStop");
	}

	guideEnt = NULL;
	guideLocked = false;
	jointGuideEnt = INVALID_JOINT;
	UpdateGuideStatus();
}

/*
================
rvWeaponMachinegun::UpdateGuideStatus
================
*/
void rvWeaponMachinegun::UpdateGuideStatus(float range) {
	// Update the zoom GUI variables
	if (isCharged) { //Dembner
		float lockStatus;
		if (guideEnt) {
			if (guideLocked) {
				lockStatus = 1.0f;
			}
			else {
				lockStatus = Min((float)(gameLocal.time - guideStartTime) / (float)guideTime, 1.0f);
			}
		}
		else {
			lockStatus = 0.0f;
		}

		if (owner == gameLocal.GetLocalPlayer()) {
			zoomGui->SetStateFloat("lockStatus", lockStatus);
			zoomGui->SetStateFloat("playerYaw", playerViewAxis.ToAngles().yaw);
		}

		if (guideEnt) {
			idVec3 diff;
			diff = guideEnt->GetPhysics()->GetOrigin() - playerViewOrigin;
			diff.NormalizeFast();
			zoomGui->SetStateFloat("lockYaw", idMath::AngleDelta(diff.ToAngles().yaw, playerViewAxis.ToAngles().yaw));
			zoomGui->SetStateString("lockRange", va("%4.2f", range));
			zoomGui->SetStateString("lockTime", va("%02d", (int)MS2SEC(guideStartTime + guideTime - gameLocal.time) + 1));
		}
	}

	// Update the guide effect
	if (guideEnt) {
		idVec3 eyePos = static_cast<idActor *>(guideEnt.GetEntity())->GetEyePosition();
		if (jointGuideEnt == INVALID_JOINT) {
			eyePos += guideEnt->GetPhysics()->GetAbsBounds().GetCenter();
		}
		else {
			idMat3	jointAxis;
			idVec3	jointLoc;
			static_cast<idAnimatedEntity *>(guideEnt.GetEntity())->GetJointWorldTransform(jointGuideEnt, gameLocal.GetTime(), jointLoc, jointAxis);
			eyePos += jointLoc;
		}
		eyePos *= 0.5f;
		if (guideEffect) {
			guideEffect->SetOrigin(eyePos);
			guideEffect->SetAxis(playerViewAxis.Transpose());
		}
		else {
			guideEffect = gameLocal.PlayEffect(
				gameLocal.GetEffect(spawnArgs, guideLocked ? "fx_guide" : "fx_guidestart"),
				eyePos, playerViewAxis.Transpose(), true, vec3_origin, false);
			if (guideEffect) {
				guideEffect->GetRenderEffect()->weaponDepthHackInViewID = owner->entityNumber + 1;
				guideEffect->GetRenderEffect()->allowSurfaceInViewID = owner->entityNumber + 1;
			}
		}
	}
	else {
		if (guideEffect) {
			guideEffect->Stop();
			guideEffect = NULL;
		}
	}
}

/*
================
rvWeaponMachinegun::Think
================
*/
void rvWeaponMachinegun::Think(void) {
	idEntity* ent;
	trace_t	  tr;

	rvWeapon::Think();

	if (!guideRange) {
		return;
	}

	if (!isCharged || IsReloading() || IsHolstered()) { //Dembner
		CancelGuide();
		return;
	}

	// Dont update the target if the current target is still alive, unhidden and we have already locked
	if (guideEnt && guideEnt->health > 0 && !guideEnt->IsHidden() && guideLocked) {
		float	range;
		range = (guideEnt->GetPhysics()->GetOrigin() - playerViewOrigin).LengthFast();
		if (range > guideRange || gameLocal.time > guideStartTime + guideTime) {
			CancelGuide();
		}
		else {
			UpdateGuideStatus(range);
			return;
		}
	}

	// Cast a ray out to the lock range
	// RAVEN BEGIN
	// ddynerman: multiple clip worlds
	gameLocal.TracePoint(owner, tr,
		playerViewOrigin,
		playerViewOrigin + playerViewAxis[0] * guideRange,
		MASK_SHOT_BOUNDINGBOX, owner);
	// RAVEN END

	if (tr.fraction >= 1.0f) {
		CancelGuide();
		return;
	}

	ent = gameLocal.entities[tr.c.entityNum];

	//if we're using a target nailable...
	if (ent->IsType(rvTarget_Nailable::GetClassType())) {
		const char* jointName = ent->spawnArgs.GetString("lock_joint");
		ent = ent->targets[0].GetEntity();
		if (!ent) {
			CancelGuide();
			return;
		}

		if (ent->GetAnimator()) {
			jointGuideEnt = ent->GetAnimator()->GetJointHandle(jointName);
		}
	}


	if (!ent->IsType(idActor::GetClassType())) {
		CancelGuide();
		return;
	}
	if (gameLocal.GetLocalPlayer() && static_cast<idActor *>(ent)->team == gameLocal.GetLocalPlayer()->team) {
		CancelGuide();
		return;
	}

	if (guideEnt != ent) {
		guideStartTime = gameLocal.time;
		guideTime = guideAquireTime;
		guideEnt = ent;
		if (zoomGui) {
			zoomGui->HandleNamedEvent("lockStart");
		}
	}
	else if (gameLocal.time > guideStartTime + guideTime) {
		// Stop the guide effect since it was just the guide_Start effect
		if (guideEffect) {
			guideEffect->Stop();
			guideEffect = NULL;
		}
		guideLocked = true;
		guideTime = guideHoldTime;
		guideStartTime = gameLocal.time;
		if (zoomGui) {
			zoomGui->HandleNamedEvent("lockAquired");
		}
	}

	UpdateGuideStatus((ent->GetPhysics()->GetOrigin() - playerViewOrigin).LengthFast());

}

/*
================
rvWeaponNailgun::OnLaunchProjectile
================
*/
void rvWeaponMachinegun::OnLaunchProjectile(idProjectile* proj) {
	rvWeapon::OnLaunchProjectile(proj);

	idGuidedProjectile* guided;
	guided = dynamic_cast<idGuidedProjectile*>(proj);
	if (guided) {
		guided->GuideTo(guideEnt, jointGuideEnt);
	}
}


/*
================
rvWeaponMachinegun::Restore
================
*/
void rvWeaponMachinegun::Restore(idRestoreGame *savefile) {
	savefile->ReadInt(chargeTime);
	savefile->ReadInt(chargeDelay);
	savefile->ReadVec2(chargeGlow);
	savefile->ReadBool(fireForced);
	savefile->ReadInt(fireHeldTime);
}

/*
================
rvWeaponMachinegun::PreSave
================
*/
void rvWeaponMachinegun::PreSave(void) {

	SetState("Idle", 4);

	StopSound(SND_CHANNEL_WEAPON, 0);
	StopSound(SND_CHANNEL_BODY, 0);
	StopSound(SND_CHANNEL_ITEM, 0);
	StopSound(SND_CHANNEL_ANY, false);

}

/*
================
rvWeaponMachinegun::PostSave
================
*/
void rvWeaponMachinegun::PostSave(void) {
}

/*
===============================================================================

States

===============================================================================
*/

CLASS_STATES_DECLARATION(rvWeaponMachinegun)
STATE("Raise", rvWeaponMachinegun::State_Raise)
STATE("Lower", rvWeaponMachinegun::State_Lower)
STATE("Idle", rvWeaponMachinegun::State_Idle)
STATE("Charge", rvWeaponMachinegun::State_Charge)
STATE("Charged", rvWeaponMachinegun::State_Charged)
STATE("Fire", rvWeaponMachinegun::State_Fire)
STATE("Flashlight", rvWeaponMachinegun::State_Flashlight)
END_CLASS_STATES

/*
================
rvWeaponMachinegun::State_Raise
================
*/
stateResult_t rvWeaponMachinegun::State_Raise(const stateParms_t& parms) {
	enum {
		RAISE_INIT,
		RAISE_WAIT,
	};
	switch (parms.stage) {
	case RAISE_INIT:
		SetStatus(WP_RISING);
		PlayAnim(ANIMCHANNEL_ALL, "raise", parms.blendFrames);
		return SRESULT_STAGE(RAISE_WAIT);

	case RAISE_WAIT:
		if (AnimDone(ANIMCHANNEL_ALL, 4)) {
			SetState("Idle", 4);
			return SRESULT_DONE;
		}
		if (wsfl.lowerWeapon) {
			SetState("Lower", 4);
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponMachinegun::State_Lower
================
*/
stateResult_t rvWeaponMachinegun::State_Lower(const stateParms_t& parms) {
	enum {
		LOWER_INIT,
		LOWER_WAIT,
		LOWER_WAITRAISE
	};
	switch (parms.stage) {
	case LOWER_INIT:
		SetStatus(WP_LOWERING);
		PlayAnim(ANIMCHANNEL_ALL, "putaway", parms.blendFrames);
		return SRESULT_STAGE(LOWER_WAIT);

	case LOWER_WAIT:
		if (AnimDone(ANIMCHANNEL_ALL, 0)) {
			SetStatus(WP_HOLSTERED);
			return SRESULT_STAGE(LOWER_WAITRAISE);
		}
		return SRESULT_WAIT;

	case LOWER_WAITRAISE:
		if (wsfl.raiseWeapon) {
			SetState("Raise", 0);
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponMachinegun::State_Idle
================
*/
stateResult_t rvWeaponMachinegun::State_Idle(const stateParms_t& parms) {
	enum {
		IDLE_INIT,
		IDLE_WAIT,
	};
	switch (parms.stage) {
	case IDLE_INIT:
		SetStatus(WP_READY);
		PlayCycle(ANIMCHANNEL_ALL, "idle", parms.blendFrames);
		return SRESULT_STAGE(IDLE_WAIT);

	case IDLE_WAIT:
		if (wsfl.lowerWeapon) {
			SetState("Lower", 4);
			return SRESULT_DONE;
		}

		if (UpdateFlashlight()) {
			return SRESULT_DONE;
		}
		if (UpdateAttack()) {
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponMachinegun::State_Charge
================
*/
stateResult_t rvWeaponMachinegun::State_Charge(const stateParms_t& parms) {
	enum {
		CHARGE_INIT,
		CHARGE_WAIT,
	};
	switch (parms.stage) {
	case CHARGE_INIT:
		viewModel->SetShaderParm(BLASTER_SPARM_CHARGEGLOW, chargeGlow[0]);
		StartSound("snd_charge", SND_CHANNEL_ITEM, 0, false, NULL);
		PlayCycle(ANIMCHANNEL_ALL, "charging", parms.blendFrames);
		return SRESULT_STAGE(CHARGE_WAIT);

	case CHARGE_WAIT:
		if (gameLocal.time - fireHeldTime < chargeTime) {
			float f;
			f = (float)(gameLocal.time - fireHeldTime) / (float)chargeTime;
			f = chargeGlow[0] + f * (chargeGlow[1] - chargeGlow[0]);
			f = idMath::ClampFloat(chargeGlow[0], chargeGlow[1], f);
			viewModel->SetShaderParm(BLASTER_SPARM_CHARGEGLOW, f);

			if (!wsfl.attack) {
				SetState("Fire", 0);
				return SRESULT_DONE;
			}

			return SRESULT_WAIT;
		}
		SetState("Charged", 4);
		//Charge status here
		isCharged = true; //-Dembner
		return SRESULT_DONE;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponMachinegun::State_Charged
================
*/
stateResult_t rvWeaponMachinegun::State_Charged(const stateParms_t& parms) {
	enum {
		CHARGED_INIT,
		CHARGED_WAIT,
	};
	switch (parms.stage) {
	case CHARGED_INIT:
		viewModel->SetShaderParm(BLASTER_SPARM_CHARGEGLOW, 1.0f);

		StopSound(SND_CHANNEL_ITEM, false);
		StartSound("snd_charge_loop", SND_CHANNEL_ITEM, 0, false, NULL);
		StartSound("snd_charge_click", SND_CHANNEL_BODY, 0, false, NULL);
		return SRESULT_STAGE(CHARGED_WAIT);

	case CHARGED_WAIT:
		if (!wsfl.attack) {
			fireForced = true;
			SetState("Fire", 0);
			isCharged = false; //Dembner
			return SRESULT_DONE;
		}
		UpdateFlashlight();
		//AttackSuper(1, spread, 0, 1.0f);

		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponMachinegun::State_Fire
================
*/
stateResult_t rvWeaponMachinegun::State_Fire(const stateParms_t& parms) {
	enum {
		FIRE_INIT,
		FIRE_WAIT,
	};
	switch (parms.stage) {
	case FIRE_INIT:

		StopSound(SND_CHANNEL_ITEM, false);
		viewModel->SetShaderParm(BLASTER_SPARM_CHARGEGLOW, 0);
		//don't fire if we're targeting a gui.
		idPlayer* player;
		player = gameLocal.GetLocalPlayer();

		//make sure the player isn't looking at a gui first
		if (player && player->GuiActive()) {
			fireHeldTime = 0;
			SetState("Lower", 0);
			return SRESULT_DONE;
		}

		if (player && !player->CanFire()) {
			fireHeldTime = 0;
			SetState("Idle", 4);
			return SRESULT_DONE;
		}



		if (gameLocal.time - fireHeldTime > chargeTime) {
			Attack(true, 2, spread, 0, 1.0f);
			PlayEffect("fx_chargedflash", barrelJointView, false);
			PlayAnim(ANIMCHANNEL_ALL, "chargedfire", parms.blendFrames);
		}
		else {
			Attack(false, 2, spread, 0, 1.0f); //2 projectiles instead of 1?!
			PlayEffect("fx_normalflash", barrelJointView, false);
			PlayAnim(ANIMCHANNEL_ALL, "fire", parms.blendFrames);
		}
		fireHeldTime = 0;

		return SRESULT_STAGE(FIRE_WAIT);

	case FIRE_WAIT:
		if (AnimDone(ANIMCHANNEL_ALL, 4)) {
			SetState("Idle", 4);
			return SRESULT_DONE;
		}
		if (UpdateFlashlight() || UpdateAttack()) {
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponMachinegun::State_Flashlight
================
*/
stateResult_t rvWeaponMachinegun::State_Flashlight(const stateParms_t& parms) {
	enum {
		FLASHLIGHT_INIT,
		FLASHLIGHT_WAIT,
	};


	switch (parms.stage) {
	case FLASHLIGHT_INIT:
		SetStatus(WP_FLASHLIGHT);
		// Wait for the flashlight anim to play		
		PlayAnim(ANIMCHANNEL_ALL, "flashlight", 0);
		return SRESULT_STAGE(FLASHLIGHT_WAIT);

	case FLASHLIGHT_WAIT:
		if (!AnimDone(ANIMCHANNEL_ALL, 4)) {
			return SRESULT_WAIT;
		}

		if (owner->IsFlashlightOn()) {
			Flashlight(false);
			AttackSuper(1, spread, 0, 1.0f); //Fires da misilles
		}
		else {
			Flashlight(true);
			AttackSuper(1, spread, 0, 1.0f); //Fires da misilles
		}

		SetState("Idle", 4);
		return SRESULT_DONE;
	}
	return SRESULT_ERROR;
}

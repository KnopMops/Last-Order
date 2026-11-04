#pragma once
#include "engine/core/Engine.h"
#include "raylib.h"
#include <string>

namespace game::chapter3 {

class WeaponAssembly {
public:
    void Reset();
    bool TryAttachPart(const std::string& itemId);
    bool CanAssemble() const;
    void CompleteAssembly(engine::core::Engine& eng);
    bool IsAssembled() const { return m_assembled; }

    bool HasBarrel() const { return m_hasBarrel; }
    bool HasStock() const { return m_hasStock; }
    bool HasTrigger() const { return m_hasTrigger; }
    int GetPartsAttachedCount() const;

    bool CanFire() const;
    bool Fire(engine::core::Engine& eng, Vector3 origin, Vector3 direction, float maxRange, Vector3& outHitPos, bool& outHitEntity);
    void AddAmmo(int shells);
    bool Reload(engine::core::Engine& eng);
    void Update(float dt);

    int GetLoadedAmmo() const { return m_loadedAmmo; }
    int GetReserveAmmo() const { return m_reserveAmmo; }
    float GetRecoilTimer() const { return m_recoilTimer; }
    bool IsReloading() const { return m_reloading; }

private:
    bool m_hasBarrel = false;
    bool m_hasStock = false;
    bool m_hasTrigger = false;
    bool m_assembled = false;

    int m_loadedAmmo = 0;
    int m_reserveAmmo = 0;
    float m_cooldown = 0.0f;
    float m_recoilTimer = 0.0f;
    bool m_reloading = false;
    float m_reloadTimer = 0.0f;
};

}


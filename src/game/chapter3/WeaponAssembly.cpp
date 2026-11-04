#include "game/chapter3/WeaponAssembly.h"
#include "raymath.h"
#include <algorithm>

namespace game::chapter3 {

void WeaponAssembly::Reset() {
    m_hasBarrel = false;
    m_hasStock = false;
    m_hasTrigger = false;
    m_assembled = false;
    m_loadedAmmo = 0;
    m_reserveAmmo = 0;
    m_cooldown = 0.0f;
    m_recoilTimer = 0.0f;
    m_reloading = false;
    m_reloadTimer = 0.0f;
}

bool WeaponAssembly::TryAttachPart(const std::string& itemId) {
    if (m_assembled) return false;
    if (itemId == "item_shotgun_barrel" && !m_hasBarrel) {
        m_hasBarrel = true;
        return true;
    }
    if (itemId == "item_shotgun_stock" && !m_hasStock) {
        m_hasStock = true;
        return true;
    }
    if (itemId == "item_shotgun_trigger" && !m_hasTrigger) {
        m_hasTrigger = true;
        return true;
    }
    return false;
}

bool WeaponAssembly::CanAssemble() const {
    return !m_assembled && m_hasBarrel && m_hasStock && m_hasTrigger;
}

int WeaponAssembly::GetPartsAttachedCount() const {
    int c = 0;
    if (m_hasBarrel) c++;
    if (m_hasStock) c++;
    if (m_hasTrigger) c++;
    return c;
}

void WeaponAssembly::CompleteAssembly(engine::core::Engine& eng) {
    if (!CanAssemble()) return;
    m_assembled = true;
    m_loadedAmmo = 2;
    eng.Audio().PlaySfx("audio/sfx/sfx_ch3_shotgun_assemble.wav", 1.0f);
}

bool WeaponAssembly::CanFire() const {
    return m_assembled && m_loadedAmmo > 0 && m_cooldown <= 0.0f && !m_reloading;
}

bool WeaponAssembly::Fire(engine::core::Engine& eng, Vector3 origin, Vector3 direction, float maxRange, Vector3& outHitPos, bool& outHitEntity) {
    if (!CanFire()) return false;

    m_loadedAmmo--;
    m_cooldown = 0.65f;
    m_recoilTimer = 0.28f;
    outHitEntity = false;

    eng.Audio().PlaySfx("audio/sfx/sfx_ch3_shotgun_blast.wav", 1.0f);
    eng.Renderer().TriggerShake(2.4f, 0.25f);

    Vector3 dir = Vector3Normalize(direction);
    outHitPos = Vector3Add(origin, Vector3Scale(dir, maxRange));
    return true;
}

void WeaponAssembly::AddAmmo(int shells) {
    m_reserveAmmo += shells;
    if (m_loadedAmmo == 0 && !m_reloading) {
        int load = std::min(m_reserveAmmo, 2);
        m_loadedAmmo += load;
        m_reserveAmmo -= load;
    }
}

bool WeaponAssembly::Reload(engine::core::Engine& eng) {
    if (!m_assembled || m_reloading || m_loadedAmmo >= 2 || m_reserveAmmo <= 0) {
        return false;
    }
    m_reloading = true;
    m_reloadTimer = 1.6f;
    eng.Audio().PlaySfx("audio/sfx/sfx_ch3_shotgun_assemble.wav", 0.7f);
    return true;
}

void WeaponAssembly::Update(float dt) {
    if (m_cooldown > 0.0f) {
        m_cooldown -= dt;
    }
    if (m_recoilTimer > 0.0f) {
        m_recoilTimer -= dt;
    }
    if (m_reloading) {
        m_reloadTimer -= dt;
        if (m_reloadTimer <= 0.0f) {
            m_reloading = false;
            int needed = 2 - m_loadedAmmo;
            int load = std::min(m_reserveAmmo, needed);
            m_loadedAmmo += load;
            m_reserveAmmo -= load;
        }
    }
}

}


#include "game/chapter3/ItemRandomizer.h"
#include <algorithm>
#include <random>

namespace game::chapter3 {

void ItemRandomizer::Init() {
    m_registry.clear();
    RegisterItem("item_shotgun_barrel", "Ствол ТОЗ-34", "Вороненые сдвоенные стволы 12 калибра.", ItemCategory::WeaponPart, "sprites/3d/ch3/item_shotgun_barrel.png");
    RegisterItem("item_shotgun_stock", "Приклад ТОЗ-34", "Ореховый приклад с резиновым затыльником.", ItemCategory::WeaponPart, "sprites/3d/ch3/item_shotgun_stock.png");
    RegisterItem("item_shotgun_trigger", "Спусковой механизм", "Ствольная коробка со спусковыми крючками.", ItemCategory::WeaponPart, "sprites/3d/ch3/item_shotgun_trigger.png");
    RegisterItem("item_shotgun_ammo", "Патроны 12 калибра", "Пачка дробовых патронов для ружья.", ItemCategory::Consumable, "sprites/3d/ch3/item_shotgun_ammo.png");
    RegisterItem("item_crowbar", "Монтировка", "Красный стальной гвоздодер для досок.", ItemCategory::Tool, "sprites/3d/ch3/item_crowbar.png");
    RegisterItem("item_wire_cutters", "Кусачки", "Слесарные кусачки с изолированными ручками.", ItemCategory::Tool, "sprites/3d/ch3/item_wire_cutters.png");
    RegisterItem("item_car_battery", "Аккумулятор 12V", "Тяжелый автомобильный аккумулятор.", ItemCategory::CarPart, "sprites/3d/ch3/item_car_battery.png");
    RegisterItem("item_car_sparkplug", "Свеча зажигания", "Свеча с медным наконечником для мотора.", ItemCategory::CarPart, "sprites/3d/ch3/item_car_sparkplug.png");
    RegisterItem("item_gas_canister", "Канистра бензина", "Металлическая канистра с топливом АИ-92.", ItemCategory::CarPart, "sprites/3d/ch3/item_crowbar.png");
    RegisterItem("item_car_key", "Ключи от машины", "Связка ключей с брелоком с эмблемой ВАЗ.", ItemCategory::Key, "sprites/3d/ch3/item_padlock_open.png");
    RegisterItem("item_padlock_key", "Ключ от замка", "Маленький латунный ключик от навесного замка.", ItemCategory::Key, "sprites/3d/ch3/item_padlock_open.png");
    RegisterItem("item_master_key", "Главный ключ дома", "Тяжелый старинный ключ от парадной двери.", ItemCategory::Key, "sprites/3d/ch3/item_padlock_open.png");
    RegisterItem("item_basement_key", "Ключ от подвала", "Ржавый ключ с биркой Мастерская.", ItemCategory::Key, "sprites/3d/ch3/item_padlock_open.png");
    RegisterItem("item_garage_key", "Ключ от гаража", "Плоский стальной ключ.", ItemCategory::Key, "sprites/3d/ch3/item_padlock_open.png");
    RegisterItem("item_wrench", "Гаечный ключ 24", "Разводной ключ для вентилей и труб.", ItemCategory::Tool, "sprites/3d/ch3/item_crowbar.png");
    RegisterItem("item_sewer_key", "Ключ от коллектора", "Граненый ключ от дренажной решетки.", ItemCategory::Key, "sprites/3d/ch3/item_padlock_open.png");
    RegisterItem("item_radio_fuse", "Предохранитель 25A", "Керамический предохранитель для радиостанции.", ItemCategory::Tool, "sprites/3d/ch3/item_car_sparkplug.png");
    RegisterItem("item_vacuum_tube", "Радиолампа ГУ-50", "Мощная генераторная лампа для передатчика.", ItemCategory::Tool, "sprites/3d/ch3/item_car_sparkplug.png");
    RegisterItem("item_flashlight", "Фонарик", "Карманный фонарь. [F] — включить. Батарейки садятся.", ItemCategory::Tool, "");
    RegisterItem("item_batteries", "Батарейки", "Две пальчиковые батарейки. [Q] — заменить в фонарике.", ItemCategory::Consumable, "");
    RegisterItem("item_code_note", "Записка с кодом", "Клочок бумаги с цифрами от домофона.", ItemCategory::Clue, "sprites/3d/ch3/item_padlock_open.png");
    SetupSlots();
}

void ItemRandomizer::RegisterItem(const std::string& id, const std::string& name, const std::string& desc, ItemCategory cat, const std::string& icon) {
    m_registry[id] = ItemDef{id, name, desc, cat, icon};
}

void ItemRandomizer::SetupSlots() {
    m_slots.clear();
    m_slots.push_back({"slot_bed_drawer", "Тумбочка у кровати", "Bedroom", {-1.9f, 3.93f, 2.15f}, "", false, false, ""});
    m_slots.push_back({"slot_bed_wardrobe", "Платяной шкаф", "Bedroom", {-5.3f, 4.3f, 2.35f}, "", false, false, ""});
    m_slots.push_back({"slot_bed_under", "Под кроватью", "Bedroom", {-4.2f, 3.4f, 3.25f}, "", false, false, ""});
    m_slots.push_back({"slot_bed_dresser", "Комод спальни", "Bedroom", {-2.2f, 4.05f, 5.4f}, "", false, false, ""});

    m_slots.push_back({"slot_hall2_closet", "Бельевой шкаф", "Hallway2F", {-0.85f, 4.3f, -5.4f}, "", false, false, ""});
    m_slots.push_back({"slot_hall2_drawer", "Консольный столик", "Hallway2F", {-1.0f, 3.95f, 0.6f}, "", false, false, ""});

    m_slots.push_back({"slot_bath_mirror", "Аптечка за зеркалом", "Bathroom", {5.8f, 4.8f, -0.55f}, "", false, false, ""});
    m_slots.push_back({"slot_bath_sink", "Тумба под раковиной", "Bathroom", {5.45f, 3.75f, -0.55f}, "", false, false, ""});

    m_slots.push_back({"slot_guest_desk", "Письменный стол", "GuestRoom", {3.7f, 3.98f, 5.35f}, "", false, false, ""});
    m_slots.push_back({"slot_guest_shelf", "Книжная полка", "GuestRoom", {5.6f, 4.3f, 1.95f}, "", false, false, ""});
    m_slots.push_back({"slot_guest_box", "Коробка с архивом", "GuestRoom", {2.1f, 3.6f, 5.5f}, "", false, false, ""});

    m_slots.push_back({"slot_attic_crate1", "Пыльный ящик у слухового окна", "Attic", {-3.2f, 7.0f, 3.9f}, "", false, true, "item_crowbar"});
    m_slots.push_back({"slot_attic_crate2", "Сундук под стропилами", "Attic", {2.6f, 6.95f, 4.7f}, "", false, false, ""});
    m_slots.push_back({"slot_attic_table", "Радиостол чердака", "Attic", {4.8f, 7.2f, -5.3f}, "", false, false, ""});

    m_slots.push_back({"slot_living_tv", "Тумба под старым ТВ", "LivingRoom", {-3.1f, 0.45f, 5.45f}, "", false, false, ""});
    m_slots.push_back({"slot_living_sofa", "Между подушек дивана", "LivingRoom", {-5.2f, 0.5f, 4.7f}, "", false, false, ""});
    m_slots.push_back({"slot_living_fire", "Каминная полка", "LivingRoom", {-3.5f, 1.32f, 2.3f}, "", false, false, ""});

    m_slots.push_back({"slot_kitchen_cup1", "Верхний кухонный шкаф", "Kitchen", {3.6f, 1.85f, -5.6f}, "", false, false, ""});
    m_slots.push_back({"slot_kitchen_cup2", "Нижний ящик гарнитура", "Kitchen", {4.8f, 0.5f, -5.35f}, "", false, false, ""});
    m_slots.push_back({"slot_kitchen_fridge", "Старый холодильник ЗИЛ", "Kitchen", {5.3f, 1.1f, -3.8f}, "", false, false, ""});
    m_slots.push_back({"slot_kitchen_oven", "Духовой шкаф плиты", "Kitchen", {2.0f, 0.4f, -5.3f}, "", false, false, ""});

    m_slots.push_back({"slot_garage_tool1", "Верхний ящик верстака", "Garage", {-12.3f, 0.9f, -1.3f}, "", false, false, ""});
    m_slots.push_back({"slot_garage_tool2", "Нижняя тумба автослесаря", "Garage", {-12.3f, 1.0f, 2.2f}, "", false, false, ""});
    m_slots.push_back({"slot_garage_trunk", "Багажник Копейки", "Garage", {-9.5f, 0.85f, -0.55f}, "", false, true, "item_crowbar"});
    m_slots.push_back({"slot_garage_glove", "Бардачок автомобиля", "Garage", {-8.6f, 1.05f, -2.9f}, "", false, false, ""});

    m_slots.push_back({"slot_base_shelf", "Стеллаж с банками", "Basement", {5.5f, -1.9f, -2.0f}, "", false, false, ""});
    m_slots.push_back({"slot_base_barrel", "Деревянная бочка", "Basement", {3.0f, -2.1f, -5.15f}, "", false, false, ""});
    m_slots.push_back({"slot_base_drawer", "Ящик оружейного стола", "Basement", {0.05f, -2.25f, -5.4f}, "", false, false, ""});
    for (auto& s : m_slots) {
        s.open = s.id == "slot_bed_drawer" || s.id == "slot_hall2_drawer" || s.id == "slot_guest_desk" || s.id == "slot_living_fire" ||
                 s.id == "slot_living_tv" || s.id == "slot_garage_tool1" || s.id == "slot_attic_table" || s.id == "slot_base_drawer";
    }
}

void ItemRandomizer::Generate(unsigned int seed) {
    SetupSlots();

    std::mt19937 rng(seed);
    int codeNum = 1000 + (rng() % 9000);
    m_keypadCode = std::to_string(codeNum);

    std::vector<std::string> criticalEarlyItems = {
        "item_crowbar",
        "item_padlock_key",
        "item_basement_key",
        "item_garage_key"
    };

    std::vector<std::string> generalItems = {
        "item_shotgun_barrel",
        "item_shotgun_stock",
        "item_shotgun_trigger",
        "item_shotgun_ammo",
        "item_shotgun_ammo",
        "item_wire_cutters",
        "item_car_battery",
        "item_car_sparkplug",
        "item_gas_canister",
        "item_car_key",
        "item_master_key",
        "item_wrench",
        "item_sewer_key",
        "item_radio_fuse",
        "item_vacuum_tube",
        "item_batteries",
        "item_batteries",
        "item_batteries",
        "item_code_note"
    };

    if (SearchSlot* start = GetSlotMut("slot_bed_drawer")) start->containedItem = "item_flashlight";

    std::vector<size_t> earlySlotIndices;
    std::vector<size_t> otherSlotIndices;

    for (size_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].id == "slot_bed_drawer") continue;
        if (!m_slots[i].locked && m_slots[i].zone != "Basement" && m_slots[i].zone != "Garage") {
            earlySlotIndices.push_back(i);
        } else {
            otherSlotIndices.push_back(i);
        }
    }

    std::shuffle(earlySlotIndices.begin(), earlySlotIndices.end(), rng);
    std::shuffle(otherSlotIndices.begin(), otherSlotIndices.end(), rng);

    for (const auto& item : criticalEarlyItems) {
        if (!earlySlotIndices.empty()) {
            size_t slotIdx = earlySlotIndices.back();
            earlySlotIndices.pop_back();
            m_slots[slotIdx].containedItem = item;
        }
    }

    std::vector<size_t> remainingSlots;
    remainingSlots.insert(remainingSlots.end(), earlySlotIndices.begin(), earlySlotIndices.end());
    remainingSlots.insert(remainingSlots.end(), otherSlotIndices.begin(), otherSlotIndices.end());
    std::shuffle(remainingSlots.begin(), remainingSlots.end(), rng);
    std::shuffle(generalItems.begin(), generalItems.end(), rng);

    for (const auto& item : generalItems) {
        if (!remainingSlots.empty()) {
            size_t slotIdx = remainingSlots.back();
            remainingSlots.pop_back();
            m_slots[slotIdx].containedItem = item;
        }
    }
}

const SearchSlot* ItemRandomizer::GetSlot(const std::string& slotId) const {
    for (const auto& s : m_slots) {
        if (s.id == slotId) return &s;
    }
    return nullptr;
}

SearchSlot* ItemRandomizer::GetSlotMut(const std::string& slotId) {
    for (auto& s : m_slots) {
        if (s.id == slotId) return &s;
    }
    return nullptr;
}

const ItemDef* ItemRandomizer::GetItemDef(const std::string& itemId) const {
    auto it = m_registry.find(itemId);
    if (it != m_registry.end()) return &it->second;
    return nullptr;
}

}


/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzFramework/Input/System/InputSystemComponent.h>

#include <AzFramework/Input/Buses/Notifications/InputSystemNotificationBus.h>

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Motion/InputDeviceMotion.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Input/Devices/Touch/InputDeviceTouch.h>
#include <AzFramework/Input/Devices/VirtualKeyboard/InputDeviceVirtualKeyboard.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    AZStd::vector<AZStd::string> GetAllMotionChannelNames()
    {
        AZStd::vector<AZStd::string> allMotionChannelNames;
        for (AZ::u32 i = 0; i < InputDeviceMotion::Acceleration::All.size(); ++i)
        {
            const InputChannelId& channelId = InputDeviceMotion::Acceleration::All[i];
            allMotionChannelNames.push_back(channelId.GetName());
        }
        for (AZ::u32 i = 0; i < InputDeviceMotion::RotationRate::All.size(); ++i)
        {
            const InputChannelId& channelId = InputDeviceMotion::RotationRate::All[i];
            allMotionChannelNames.push_back(channelId.GetName());
        }
        for (AZ::u32 i = 0; i < InputDeviceMotion::MagneticField::All.size(); ++i)
        {
            const InputChannelId& channelId = InputDeviceMotion::MagneticField::All[i];
            allMotionChannelNames.push_back(channelId.GetName());
        }
        for (AZ::u32 i = 0; i < InputDeviceMotion::Orientation::All.size(); ++i)
        {
            const InputChannelId& channelId = InputDeviceMotion::Orientation::All[i];
            allMotionChannelNames.push_back(channelId.GetName());
        }
        return allMotionChannelNames;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    class InputSystemNotificationBusBehaviorHandler
        : public InputSystemNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        AZ_EBUS_BEHAVIOR_BINDER(InputSystemNotificationBusBehaviorHandler, "{2F3417A3-41FD-4FBB-B0B6-F154F068F4F8}", AZ::SystemAllocator
            , OnPreInputUpdate
            , OnPostInputUpdate
        );

        ////////////////////////////////////////////////////////////////////////////////////////////
        void OnPreInputUpdate() override
        {
            Call(FN_OnPreInputUpdate);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////
        void OnPostInputUpdate() override
        {
            Call(FN_OnPostInputUpdate);
        }
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<InputSystemComponent, AZ::Component>()
                ->Version(1)
                ->Field("GamepadsEnabled", &InputSystemComponent::m_gamepadsEnabled)
                ->Field("KeyboardEnabled", &InputSystemComponent::m_keyboardEnabled)
                ->Field("MotionEnabled", &InputSystemComponent::m_motionEnabled)
                ->Field("MouseEnabled", &InputSystemComponent::m_mouseEnabled)
                ->Field("TouchEnabled", &InputSystemComponent::m_touchEnabled)
                ->Field("VirtualKeyboardEnabled", &InputSystemComponent::m_virtualKeyboardEnabled)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<InputSystemComponent>(
                    "Input System", "Controls which core input devices are made available")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Engine")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                    ->DataElement(AZ::Edit::UIHandlers::SpinBox, &InputSystemComponent::m_gamepadsEnabled, "Gamepads", "The number of game-pads enabled.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0)
                        ->Attribute(AZ::Edit::Attributes::Max, 4)
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &InputSystemComponent::m_keyboardEnabled, "Keyboard", "Is keyboard input enabled?")
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &InputSystemComponent::m_motionEnabled, "Motion", "Is motion input enabled?")
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &InputSystemComponent::m_mouseEnabled, "Mouse", "Is mouse input enabled?")
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &InputSystemComponent::m_touchEnabled, "Touch", "Is touch enabled?")
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &InputSystemComponent::m_virtualKeyboardEnabled, "Virtual Keyboard", "Is the virtual keyboard enabled?")
                ;
            }
        }

        if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<InputSystemNotificationBus>("InputSystemNotificationBus")
                ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::ExcludeFlags::Preview)
                ->Attribute(AZ::Script::Attributes::Category, "Input")
                ->Handler<InputSystemNotificationBusBehaviorHandler>()
                ;

            behaviorContext->EBus<InputSystemRequestBus>("InputSystemRequestBus")
                ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::ExcludeFlags::Preview)
                ->Attribute(AZ::Script::Attributes::Category, "Input")
                ->Event("RecreateEnabledInputDevices", &InputSystemRequestBus::Events::RecreateEnabledInputDevices)
                ;
        }

        InputChannelId::Reflect(context);
        InputDeviceId::Reflect(context);
        InputChannel::Reflect(context);
        InputDevice::Reflect(context);

        InputDeviceGamepad::Reflect(context);
        InputDeviceKeyboard::Reflect(context);
        InputDeviceMotion::Reflect(context);
        InputDeviceMouse::Reflect(context);
        InputDeviceTouch::Reflect(context);
        InputDeviceVirtualKeyboard::Reflect(context);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("InputSystemService", 0x5438d51a));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("InputSystemService", 0x5438d51a));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputSystemComponent::InputSystemComponent()
        : m_gamepads()
        , m_keyboard()
        , m_motion()
        , m_mouse()
        , m_touch()
        , m_virtualKeyboard()
        , m_gamepadsEnabled(4)
        , m_keyboardEnabled(true)
        , m_motionEnabled(true)
        , m_mouseEnabled(true)
        , m_touchEnabled(true)
        , m_virtualKeyboardEnabled(true)
        , m_currentlyUpdatingInputDevices(false)
        , m_recreateInputDevicesAfterUpdate(false)
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputSystemComponent::~InputSystemComponent()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::Activate()
    {
        // Create all enabled input devices
        CreateEnabledInputDevices();

        InputSystemRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        InputSystemRequestBus::Handler::BusDisconnect();

        // Destroy all enabled input devices
        DestroyEnabledInputDevices();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    int InputSystemComponent::GetTickOrder()
    {
        return AZ::ComponentTickBus::TICK_INPUT;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::OnTick(float /*deltaTime*/, AZ::ScriptTimePoint /*scriptTimePoint*/)
    {
        TickInput();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::TickInput()
    {
        InputSystemNotificationBus::Broadcast(&InputSystemNotifications::OnPreInputUpdate);
        m_currentlyUpdatingInputDevices = true;
        InputDeviceRequestBus::Broadcast(&InputDeviceRequests::TickInputDevice);
        m_currentlyUpdatingInputDevices = false;
        InputSystemNotificationBus::Broadcast(&InputSystemNotifications::OnPostInputUpdate);

        if (m_recreateInputDevicesAfterUpdate)
        {
            CreateEnabledInputDevices();
            m_recreateInputDevicesAfterUpdate = false;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::RecreateEnabledInputDevices()
    {
        if (m_currentlyUpdatingInputDevices)
        {
            // Delay the request until we've finished updating to protect against getting called in
            // response to an input event, in which case calling CreateEnabledInputDevices here will
            // cause a crash (when the stack unwinds back up to the device which dispatced the event
            // but was then destroyed). An unlikely (but possible) scenario we must protect against.
            m_recreateInputDevicesAfterUpdate = true;
        }
        else
        {
            CreateEnabledInputDevices();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::CreateEnabledInputDevices()
    {
        DestroyEnabledInputDevices();

        m_gamepads.resize(m_gamepadsEnabled);
        for (AZ::u32 i = 0; i < m_gamepadsEnabled; ++i)
        {
            m_gamepads[i].reset(aznew InputDeviceGamepad(i));
        }

        m_keyboard.reset(m_keyboardEnabled ? aznew InputDeviceKeyboard() : nullptr);
        m_motion.reset(m_motionEnabled ? aznew InputDeviceMotion() : nullptr);
        m_mouse.reset(m_mouseEnabled ? aznew InputDeviceMouse() : nullptr);
        m_touch.reset(m_touchEnabled ? aznew InputDeviceTouch() : nullptr);
        m_virtualKeyboard.reset(m_virtualKeyboardEnabled ? aznew InputDeviceVirtualKeyboard() : nullptr);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputSystemComponent::DestroyEnabledInputDevices()
    {
        m_virtualKeyboard.reset(nullptr);
        m_touch.reset(nullptr);
        m_mouse.reset(nullptr);
        m_motion.reset(nullptr);
        m_keyboard.reset(nullptr);
        m_gamepads.clear();
    }
} // namespace AzFramework

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

// include required headers
#include "GameControllerWindow.h"

#include "AnimGraphPlugin.h"
#include "ParameterWindow.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QString>

#include <IEditor.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include "../../../../EMStudioSDK/Source/EMStudioManager.h"
#include "../../../../EMStudioSDK/Source/MainWindow.h"
#include <MysticQt/Source/DialogStack.h>
#include <MysticQt/Source/LinkWidget.h>
#include <MCore/Source/LogManager.h>
#include <EMotionFX/Source/ActorInstance.h>
#include <EMotionFX/Source/BlendTreeParameterNode.h>
#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/AnimGraphManager.h>
#include <EMotionFX/Source/AnimGraphStateMachine.h>
#include <EMotionFX/Source/AnimGraphNode.h>
#include <EMotionFX/Source/AnimGraphInstance.h>
#include <EMotionFX/CommandSystem/Source/AnimGraphParameterCommands.h>
#include "BlendNodeSelectionWindow.h"


namespace EMStudio
{
    // constructor
    GameControllerWindow::GameControllerWindow(AnimGraphPlugin* plugin, QWidget* parent)
        : QWidget(parent)
    {
        mPlugin                     = plugin;
        mAnimGraph                  = nullptr;
        mDynamicWidget              = nullptr;
        mPresetNameLineEdit         = nullptr;
        mParameterGridLayout        = nullptr;
        mDeadZoneValueLabel         = nullptr;
        mButtonGridLayout           = nullptr;
        mDeadZoneSlider             = nullptr;
        mPresetComboBox             = nullptr;
        mInterfaceTimerID           = MCORE_INVALIDINDEX32;
        mGameControllerTimerID      = MCORE_INVALIDINDEX32;
        mString.reserve(4096);
    #ifdef HAS_GAME_CONTROLLER
        mGameController         = nullptr;
    #endif

        Init();
    }


    // destructor
    GameControllerWindow::~GameControllerWindow()
    {
        // stop the timers
        mInterfaceTimer.stop();
        mGameControllerTimer.stop();

        // unregister the command callbacks and get rid of the memory
        GetCommandManager()->RemoveCommandCallback(mCreateCallback, false);
        GetCommandManager()->RemoveCommandCallback(mRemoveCallback, false);
        GetCommandManager()->RemoveCommandCallback(mAdjustCallback, false);
        GetCommandManager()->RemoveCommandCallback(mSelectCallback, false);
        GetCommandManager()->RemoveCommandCallback(mUnselectCallback, false);
        GetCommandManager()->RemoveCommandCallback(mClearSelectionCallback, false);
        delete mCreateCallback;
        delete mRemoveCallback;
        delete mAdjustCallback;
        delete mClearSelectionCallback;
        delete mSelectCallback;
        delete mUnselectCallback;

        // get rid of the game controller
    #ifdef HAS_GAME_CONTROLLER
        mGameController->Shutdown();
        delete mGameController;
    #endif
    }


    // initialize the game controller window
    void GameControllerWindow::Init()
    {
        // create the callbacks
        mCreateCallback             = new CommandCreateBlendParameterCallback(false);
        mRemoveCallback             = new CommandRemoveBlendParameterCallback(false);
        mAdjustCallback             = new CommandAdjustBlendParameterCallback(false);
        mSelectCallback             = new CommandSelectCallback(false);
        mUnselectCallback           = new CommandUnselectCallback(false);
        mClearSelectionCallback     = new CommandClearSelectionCallback(false);

        // hook the callbacks to the commands
        GetCommandManager()->RegisterCommandCallback("AnimGraphCreateParameter", mCreateCallback);
        GetCommandManager()->RegisterCommandCallback("AnimGraphRemoveParameter", mRemoveCallback);
        GetCommandManager()->RegisterCommandCallback("AnimGraphAdjustParameter", mAdjustCallback);
        GetCommandManager()->RegisterCommandCallback("Select", mSelectCallback);
        GetCommandManager()->RegisterCommandCallback("Unselect", mUnselectCallback);
        GetCommandManager()->RegisterCommandCallback("ClearSelection", mClearSelectionCallback);

        InitGameController();

        QVBoxLayout* layout = new QVBoxLayout();
        layout->setMargin(0);
        setLayout(layout);

        // create the dialog stack
        mDialogStack = new MysticQt::DialogStack();
        layout->addWidget(mDialogStack);

        // add the game controller
        mGameControllerComboBox = new MysticQt::ComboBox();
        UpdateGameControllerComboBox();

        QHBoxLayout* gameControllerLayout = new QHBoxLayout();
        gameControllerLayout->setMargin(0);
        QLabel* activeControllerLabel = new QLabel("Active Controller:");
        activeControllerLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        gameControllerLayout->addWidget(activeControllerLabel);
        gameControllerLayout->addWidget(mGameControllerComboBox);
        gameControllerLayout->addWidget(EMStudioManager::MakeSeperatorLabel(1, 20));

        // create the presets interface
        QHBoxLayout* horizontalLayout = new QHBoxLayout();
        horizontalLayout->setMargin(0);

        mPresetComboBox     = new MysticQt::ComboBox();
        mAddPresetButton    = new QPushButton();
        mRemovePresetButton = new QPushButton();
        mPresetNameLineEdit = new QLineEdit();

        connect(mPresetComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnPresetComboBox(int)));
        connect(mAddPresetButton, SIGNAL(clicked()), this, SLOT(OnAddPresetButton()));
        connect(mRemovePresetButton, SIGNAL(clicked()), this, SLOT(OnRemovePresetButton()));
        connect(mPresetNameLineEdit, SIGNAL(editingFinished()), this, SLOT(OnPresetNameChanged()));
        connect(mPresetNameLineEdit, SIGNAL(textEdited(const QString&)), this, SLOT(OnPresetNameEdited(const QString&)));

        EMStudioManager::MakeTransparentButton(mAddPresetButton, "/Images/Icons/Plus.png", "Add a game controller preset");
        EMStudioManager::MakeTransparentButton(mRemovePresetButton, "/Images/Icons/Remove.png", "Remove a game controller preset");

        QHBoxLayout* buttonsLayout = new QHBoxLayout();
        buttonsLayout->addWidget(mAddPresetButton);
        buttonsLayout->addWidget(mRemovePresetButton);
        buttonsLayout->setSpacing(0);
        buttonsLayout->setMargin(0);

        horizontalLayout->addWidget(new QLabel("Preset:"));
        horizontalLayout->addWidget(mPresetComboBox);
        horizontalLayout->addLayout(buttonsLayout);
        horizontalLayout->addWidget(mPresetNameLineEdit);

        gameControllerLayout->addLayout(horizontalLayout);
        QWidget* dummyWidget = new QWidget();
        dummyWidget->setObjectName("StyledWidgetDark");
        dummyWidget->setLayout(gameControllerLayout);
        mDialogStack->Add(dummyWidget, "Game Controller And Preset Selection");
        connect(mGameControllerComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnGameControllerComboBox(int)));

        DisablePresetInterface();
        AutoSelectGameController();

        connect(GetMainWindow(), SIGNAL(HardwareChangeDetected()), this, SLOT(HardwareChangeDetected()));
    }


    // automatically selec the game controller in the combo box
    void GameControllerWindow::AutoSelectGameController()
    {
    #ifdef HAS_GAME_CONTROLLER
        // this will call ReInit();
        if (mGameController->GetDeviceNameString().empty() == false && mGameControllerComboBox->count() > 1)
        {
            mGameControllerComboBox->setCurrentIndex(1);
        }
        else
        {
            mGameControllerComboBox->setCurrentIndex(0);
        }
    #endif
    }


    // initialize the game controller
    void GameControllerWindow::InitGameController()
    {
    #ifdef HAS_GAME_CONTROLLER
        // create the game controller object
        if (mGameController == nullptr)
        {
            mGameController = new GameController();
        }

        // Call mainWindow->window() to make sure you get the top level window which the mainWindow might not in fact be.
        //IEditor* editor = nullptr;
        //AzToolsFramework::EditorRequests::Bus::BroadcastResult(editor, &AzToolsFramework::EditorRequests::GetEditor);
        //QMainWindow* mainWindow = editor->GetEditorMainWindow();
        //HWND hWnd = reinterpret_cast<HWND>( mainWindow->window()->winId() );
        HWND hWnd = nullptr;
        if (mGameController->Init(hWnd) == false)
        {
            MCore::LogError("Cannot initialize game controller.");
        }
    #endif
    }


    void GameControllerWindow::UpdateGameControllerComboBox()
    {
        // clear it and add the none option
        mGameControllerComboBox->clear();
        mGameControllerComboBox->addItem(NO_GAMECONTROLLER_NAME);

        // add the gamepad in case it is valid and the device name is not empty
    #ifdef HAS_GAME_CONTROLLER
        if (mGameController->GetIsValid() && mGameController->GetDeviceNameString().empty() == false)
        {
            mGameControllerComboBox->addItem(mGameController->GetDeviceName());
        }
    #endif

        // always adjust the size of the combobox to the currently selected text
        mGameControllerComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    }


    // adjust the game controller combobox value
    void GameControllerWindow::OnGameControllerComboBox(int value)
    {
        MCORE_UNUSED(value);

        //MysticQt::ComboBox* combo = qobject_cast<MysticQt::ComboBox*>( sender() );
        //String stringValue = combo->currentText().toAscii().data();
        ReInit();

        // update the parameter window
        mPlugin->GetParameterWindow()->Init();
    }


    void GameControllerWindow::DisablePresetInterface()
    {
        mPresetComboBox->blockSignals(true);
        mPresetComboBox->clear();
        mPresetComboBox->blockSignals(false);

        mPresetNameLineEdit->blockSignals(true);
        mPresetNameLineEdit->setText("");
        mPresetNameLineEdit->blockSignals(false);

        mPresetComboBox->setEnabled(false);
        mPresetNameLineEdit->setEnabled(false);
        mAddPresetButton->setEnabled(false);
        mRemovePresetButton->setEnabled(false);
    }


    // reint the game controller window
    void GameControllerWindow::ReInit()
    {
        // get the anim graph
        EMotionFX::AnimGraph* animGraph = mPlugin->GetActiveAnimGraph();
        mAnimGraph = animGraph;

        // remove all existing items
        if (mDynamicWidget)
        {
            mDialogStack->Remove(mDynamicWidget);
        }
        mDynamicWidget = nullptr;
        mInterfaceTimer.stop();
        mGameControllerTimer.stop();

        // check if we need to recreate the dynamic widget
    #ifdef HAS_GAME_CONTROLLER
        if (mGameController->GetIsValid() == false || mGameControllerComboBox->currentText() != mGameController->GetDeviceName())
        {
            DisablePresetInterface();
            return;
        }
    #else
        DisablePresetInterface();
        return;
    #endif

        if (animGraph == nullptr)
        {
            DisablePresetInterface();
            return;
        }

        // create the dynamic widget
        mDynamicWidget = new QWidget();
        mDynamicWidget->setObjectName("StyledWidgetDark");

        // get the game controller settings from the anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = animGraph->GetGameControllerSettings();

        // in case there is no preset yet create a default one
        uint32 numPresets = gameControllerSettings->GetNumPresets();
        if (numPresets == 0)
        {
            EMotionFX::AnimGraphGameControllerSettings::Preset* preset = EMotionFX::AnimGraphGameControllerSettings::Preset::Create("Default");
            gameControllerSettings->AddPreset(preset);
            gameControllerSettings->SetActivePreset(preset);
            numPresets = 1;
        }

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();

        // create the parameter grid layout
        mParameterGridLayout = new QGridLayout();
        mParameterGridLayout->setAlignment(Qt::AlignTop);
        mParameterGridLayout->setMargin(0);

        // add all parameters
        //  uint32 startRow = 0;
        uint32 i;
        mParameterInfos.Clear();
        const uint32 numParameters = animGraph->GetNumParameters();
        mParameterInfos.Reserve(numParameters);
        for (i = 0; i < numParameters; ++i)
        {
            // get the attribute settings
            MCore::AttributeSettings* attributeSettings = animGraph->GetParameter(i);
            if (attributeSettings->GetDefaultValue()->GetType() != MCore::AttributeFloat::TYPE_ID && attributeSettings->GetDefaultValue()->GetType() != MCore::AttributeVector2::TYPE_ID)
            {
                continue;
            }

            EMotionFX::AnimGraphGameControllerSettings::ParameterInfo* settingsInfo = activePreset->FindParameterInfo(attributeSettings->GetName());
            if (settingsInfo == nullptr)
            {
                continue;
            }

            // add the parameter name to the layout
            AZStd::string labelString = attributeSettings->GetName();
            labelString += ":";
            QLabel* label = new QLabel(labelString.c_str());
            label->setToolTip(attributeSettings->GetDescription());
            label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            mParameterGridLayout->addWidget(label, i, 0);

            // add the axis combo box to the layout
            MysticQt::ComboBox* axesComboBox = new MysticQt::ComboBox();
            axesComboBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            axesComboBox->addItem("None");

            // iterate over the elements and add the ones which are present on the current game controller to the combo box
            uint32 selectedComboItem = 0;
        #ifdef HAS_GAME_CONTROLLER
            if (attributeSettings->GetDefaultValue()->GetType() == MCore::AttributeFloat::TYPE_ID)
            {
                uint32 numPresentElements = 0;
                for (uint32 j = 0; j < GameController::NUM_ELEMENTS; ++j)
                {
                    // check if the element is present and add it to the combo box if yes
                    if (mGameController->GetIsPresent(j))
                    {
                        // add the name of the element to the combo box
                        axesComboBox->addItem(mGameController->GetElementEnumName(j));

                        // in case the current element is the one the parameter is assigned to, remember the correct index
                        if (j == settingsInfo->mAxis)
                        {
                            selectedComboItem = numPresentElements + 1;
                        }

                        // increase the number of present elements on the plugged in game controller
                        numPresentElements++;
                    }
                }
            }
            else
            if (attributeSettings->GetDefaultValue()->GetType() == MCore::AttributeVector2::TYPE_ID)
            {
                uint32 numPresentElements = 0;
                if (mGameController->GetIsPresent(GameController::ELEM_POS_X) && mGameController->GetIsPresent(GameController::ELEM_POS_Y))
                {
                    axesComboBox->addItem("Pos XY");
                    if (settingsInfo->mAxis == 0)
                    {
                        selectedComboItem = numPresentElements + 1;
                    }
                    numPresentElements++;
                }

                if (mGameController->GetIsPresent(GameController::ELEM_ROT_X) && mGameController->GetIsPresent(GameController::ELEM_ROT_Y))
                {
                    axesComboBox->addItem("Rot XY");
                    if (settingsInfo->mAxis == 1)
                    {
                        selectedComboItem = numPresentElements + 1;
                    }
                    numPresentElements++;
                }
            }
        #endif
            connect(axesComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnAxisComboBox(int)));

            // select the given axis in the combo box or select none if there is no assignment yet or the assigned axis wasn't found on the current game controller
            axesComboBox->setCurrentIndex(selectedComboItem);
            mParameterGridLayout->addWidget(axesComboBox, i, 1);

            // add the mode combo box to the layout
            MysticQt::ComboBox* modeComboBox = new MysticQt::ComboBox();
            modeComboBox->addItem("Standard Mode");
            modeComboBox->addItem("Zero To One Mode");
            modeComboBox->addItem("Parameter Range Mode");
            modeComboBox->addItem("Positive Param Range Mode");
            modeComboBox->addItem("Negative Param Range Mode");
            modeComboBox->addItem("Rotate Character");
            modeComboBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            connect(modeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnParameterModeComboBox(int)));
            modeComboBox->setCurrentIndex(settingsInfo->mMode);
            mParameterGridLayout->addWidget(modeComboBox, i, 2);

            // add the invert checkbox to the layout
            QHBoxLayout* invertCheckBoxLayout = new QHBoxLayout();
            invertCheckBoxLayout->setMargin(0);
            QLabel* invertLabel = new QLabel("Invert");
            invertCheckBoxLayout->addWidget(invertLabel);
            QCheckBox* invertCheckbox = new QCheckBox();
            invertLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            invertCheckbox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            connect(invertCheckbox, SIGNAL(stateChanged(int)), this, SLOT(OnInvertCheckBoxChanged(int)));
            invertCheckbox->setCheckState(settingsInfo->mInvert ? Qt::Checked : Qt::Unchecked);
            invertCheckBoxLayout->addWidget(invertCheckbox);
            mParameterGridLayout->addLayout(invertCheckBoxLayout, i, 3);

            // add the current value edit field to the layout
            QLineEdit* valueEdit = new QLineEdit();
            valueEdit->setEnabled(false);
            valueEdit->setReadOnly(true);
            valueEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            valueEdit->setMinimumWidth(70);
            valueEdit->setMaximumWidth(70);
            mParameterGridLayout->addWidget(valueEdit, i, 4);

            // create the parameter info and add it to the array
            ParameterInfo paramInfo;
            paramInfo.mAttributeSettings = attributeSettings;
            paramInfo.mAxis             = axesComboBox;
            paramInfo.mMode             = modeComboBox;
            paramInfo.mInvert           = invertCheckbox;
            paramInfo.mValue            = valueEdit;
            mParameterInfos.Add(paramInfo);

            // update the interface
            UpdateParameterInterface(&paramInfo);
        }

        // create the button layout
        mButtonGridLayout = new QGridLayout();
        mButtonGridLayout->setAlignment(Qt::AlignTop);
        mButtonGridLayout->setMargin(0);

        // clear the button infos
        mButtonInfos.Clear();

        // get the number of buttons and iterate through them
    #ifdef HAS_GAME_CONTROLLER
        const uint32 numButtons = mGameController->GetNumButtons();
        for (i = 0; i < numButtons; ++i)
        {
            EMotionFX::AnimGraphGameControllerSettings::ButtonInfo* settingsInfo = activePreset->FindButtonInfo(i);
            MCORE_ASSERT(settingsInfo);

            // add the button name to the layout
            mString = AZStd::string::format("Button %s%d", (i < 10) ? "0" : "", i);
            QLabel* nameLabel = new QLabel(mString.c_str());
            nameLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            mButtonGridLayout->addWidget(nameLabel, i, 0);

            // add the mode combo box to the layout
            MysticQt::ComboBox* modeComboBox = new MysticQt::ComboBox();
            modeComboBox->addItem("None");
            modeComboBox->addItem("Switch To State Mode");
            modeComboBox->addItem("Toggle Bool Parameter Mode");
            modeComboBox->addItem("Enable Bool While Pressed Mode");
            modeComboBox->addItem("Disable Bool While Pressed Mode");
            modeComboBox->addItem("Enable Bool For One Frame Only");
            //modeComboBox->addItem( "Execute Script Mode" );
            modeComboBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            connect(modeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnButtonModeComboBox(int)));
            modeComboBox->setCurrentIndex(settingsInfo->mMode);
            mButtonGridLayout->addWidget(modeComboBox, i, 1);

            mButtonInfos.Add(ButtonInfo(i, modeComboBox));

            // reinit the dynamic part of the button layout
            ReInitButtonInterface(i);
        }

        // real time preview of the controller
        mPreviewLabels.Clear();
        mPreviewLabels.Resize(GameController::NUM_ELEMENTS + 1);
        QVBoxLayout* realtimePreviewLayout = new QVBoxLayout();
        QGridLayout* previewGridLayout = new QGridLayout();
        previewGridLayout->setAlignment(Qt::AlignTop);
        previewGridLayout->setSpacing(5);
        uint32 realTimePreviewLabelCounter = 0;
        for (i = 0; i < GameController::NUM_ELEMENTS; ++i)
        {
            if (mGameController->GetIsPresent(i))
            {
                QLabel* elementNameLabel = new QLabel(mGameController->GetElementEnumName(i));
                elementNameLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                previewGridLayout->addWidget(elementNameLabel, realTimePreviewLabelCounter, 0);

                mPreviewLabels[i] = new QLabel();
                previewGridLayout->addWidget(mPreviewLabels[i], realTimePreviewLabelCounter, 1, Qt::AlignLeft);

                realTimePreviewLabelCounter++;
            }
            else
            {
                mPreviewLabels[i] = nullptr;
            }
        }
        realtimePreviewLayout->addLayout(previewGridLayout);

        // add the special case label for the pressed buttons
        mPreviewLabels[GameController::NUM_ELEMENTS] = new QLabel();
        QLabel* realtimeButtonNameLabel = new QLabel("Buttons");
        realtimeButtonNameLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        previewGridLayout->addWidget(realtimeButtonNameLabel, realTimePreviewLabelCounter, 0);
        previewGridLayout->addWidget(mPreviewLabels[GameController::NUM_ELEMENTS], realTimePreviewLabelCounter, 1, Qt::AlignLeft);

        // add the dead zone elements
        QHBoxLayout* deadZoneLayout = new QHBoxLayout();
        deadZoneLayout->setMargin(0);

        QLabel* deadZoneLabel = new QLabel("Dead Zone");
        deadZoneLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        previewGridLayout->addWidget(deadZoneLabel, realTimePreviewLabelCounter + 1, 0);

        mDeadZoneSlider = new MysticQt::Slider(Qt::Horizontal);
        mDeadZoneSlider->setRange(1, 90);
        mDeadZoneSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        deadZoneLayout->addWidget(mDeadZoneSlider);

        mDeadZoneValueLabel = new QLabel();
        deadZoneLayout->addWidget(mDeadZoneValueLabel);
        previewGridLayout->addLayout(deadZoneLayout, realTimePreviewLabelCounter + 1, 1);

        mDeadZoneSlider->setValue(mGameController->GetDeadZone() * 100);
        mString = AZStd::string::format("%.2f", mGameController->GetDeadZone());
        mDeadZoneValueLabel->setText(mString.c_str());
        connect(mDeadZoneSlider, SIGNAL(valueChanged(int)), this, SLOT(OnDeadZoneSliderChanged(int)));
    #endif

        // start the timers
        mInterfaceTimer.start(1000 / 20.0f, this);
        mInterfaceTimerID = mInterfaceTimer.timerId();
        mGameControllerTimer.start(1000 / 100.0f, this);
        mGameControllerTimerID = mGameControllerTimer.timerId();

        // create the vertical layout for the parameter and the button setup
        QVBoxLayout* verticalLayout = new QVBoxLayout();
        verticalLayout->setAlignment(Qt::AlignTop);

        ////////////////////////////

        mPresetComboBox->blockSignals(true);
        mPresetComboBox->clear();
        // add the presets to the combo box
        for (i = 0; i < numPresets; ++i)
        {
            mPresetComboBox->addItem(gameControllerSettings->GetPreset(i)->GetName());
        }

        // select the active preset
        const uint32 activePresetIndex = gameControllerSettings->GetActivePresetIndex();
        if (activePresetIndex != MCORE_INVALIDINDEX32)
        {
            mPresetComboBox->setCurrentIndex(activePresetIndex);
        }
        mPresetComboBox->blockSignals(false);

        // set the name of the active preset
        if (gameControllerSettings->GetActivePreset())
        {
            mPresetNameLineEdit->blockSignals(true);
            mPresetNameLineEdit->setText(gameControllerSettings->GetActivePreset()->GetName());
            mPresetNameLineEdit->blockSignals(false);
        }

        mPresetComboBox->setEnabled(true);
        mPresetNameLineEdit->setEnabled(true);
        mAddPresetButton->setEnabled(true);
        mRemovePresetButton->setEnabled(true);

        ////////////////////////////

        // construct the parameters name layout
        QHBoxLayout* parameterNameLayout = new QHBoxLayout();
        QLabel* label = new QLabel("Parameters");
        label->setStyleSheet("color: rgb(244, 156, 28);");
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        parameterNameLayout->addWidget(label);

        // add spacer item
        QWidget* spacerItem = new QWidget();
        spacerItem->setStyleSheet("background-color: qlineargradient(x1:0, y1:0, x2:1, y2:, stop:0 rgb(55, 55, 55), stop:0.5 rgb(144, 152, 160), stop:1 rgb(55, 55, 55));");
        spacerItem->setMinimumHeight(1);
        spacerItem->setMaximumHeight(1);
        spacerItem->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        parameterNameLayout->addWidget(spacerItem);


        // construct the buttons name layout
        QHBoxLayout* buttonNameLayout = new QHBoxLayout();
        label = new QLabel("Buttons");
        label->setStyleSheet("color: rgb(244, 156, 28);");
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        buttonNameLayout->addWidget(label);

        // add spacer item
        spacerItem = new QWidget();
        spacerItem->setStyleSheet("background-color: qlineargradient(x1:0, y1:0, x2:1, y2:, stop:0 rgb(55, 55, 55), stop:0.5 rgb(144, 152, 160), stop:1 rgb(55, 55, 55));");
        spacerItem->setMinimumHeight(1);
        spacerItem->setMaximumHeight(1);
        spacerItem->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        buttonNameLayout->addWidget(spacerItem);

        verticalLayout->addLayout(parameterNameLayout);
        verticalLayout->addLayout(mParameterGridLayout);
        verticalLayout->addLayout(buttonNameLayout);
        verticalLayout->addLayout(mButtonGridLayout);

        // main dynamic widget layout
        QHBoxLayout* dynamicWidgetLayout = new QHBoxLayout();
        dynamicWidgetLayout->setMargin(0);

        // add the left side
        dynamicWidgetLayout->addLayout(verticalLayout);

        // add the realtime preview window to the dynamic widget layout
    #ifdef HAS_GAME_CONTROLLER
        QWidget* realTimePreviewWidget = new QWidget();
        realTimePreviewWidget->setMinimumWidth(200);
        realTimePreviewWidget->setMaximumWidth(200);
        realTimePreviewWidget->setStyleSheet("background-color: rgb(65, 65, 65);");
        realTimePreviewWidget->setLayout(realtimePreviewLayout);
        dynamicWidgetLayout->addWidget(realTimePreviewWidget);
        dynamicWidgetLayout->setAlignment(realTimePreviewWidget, Qt::AlignTop);
    #endif
        mDynamicWidget->setLayout(dynamicWidgetLayout);

        mDialogStack->Add(mDynamicWidget, "Game Controller Mapping", false, true);
    }


    void GameControllerWindow::OnDeadZoneSliderChanged(int value)
    {
    #ifdef HAS_GAME_CONTROLLER
        mGameController->SetDeadZone(value * 0.01f);
        mString = AZStd::string::format("%.2f", value * 0.01f);
        mDeadZoneValueLabel->setText(mString.c_str());
    #else
        MCORE_UNUSED(value);
    #endif
    }


    GameControllerWindow::ButtonInfo* GameControllerWindow::FindButtonInfo(QWidget* widget)
    {
        // get the number of button infos and iterate through them
        const uint32 numButtonInfos = mButtonInfos.GetLength();
        for (uint32 i = 0; i < numButtonInfos; ++i)
        {
            if (mButtonInfos[i].mWidget == widget)
            {
                return &mButtonInfos[i];
            }
        }

        // return failure
        return nullptr;
    }


    GameControllerWindow::ParameterInfo* GameControllerWindow::FindParamInfoByModeComboBox(MysticQt::ComboBox* comboBox)
    {
        // get the number of parameter infos and iterate through them
        const uint32 numParamInfos = mParameterInfos.GetLength();
        for (uint32 i = 0; i < numParamInfos; ++i)
        {
            if (mParameterInfos[i].mMode == comboBox)
            {
                return &mParameterInfos[i];
            }
        }

        // return failure
        return nullptr;
    }


    // find the interface parameter info based on the attribute info
    GameControllerWindow::ParameterInfo* GameControllerWindow::FindButtonInfoByAttributeInfo(MCore::AttributeSettings* attributeSettings)
    {
        // get the number of parameter infos and iterate through them
        const uint32 numParamInfos = mParameterInfos.GetLength();
        for (uint32 i = 0; i < numParamInfos; ++i)
        {
            if (mParameterInfos[i].mAttributeSettings == attributeSettings)
            {
                return &mParameterInfos[i];
            }
        }

        // return failure
        return nullptr;
    }


    // enable/disable controls for a given parameter
    void GameControllerWindow::UpdateParameterInterface(ParameterInfo* parameterInfo)
    {
        int comboAxisIndex = parameterInfo->mAxis->currentIndex();
        if (comboAxisIndex == 0) // None
        {
            parameterInfo->mMode->setEnabled(false);
            parameterInfo->mInvert->setEnabled(false);
            parameterInfo->mValue->setEnabled(false);
            parameterInfo->mValue->setText("");
        }
        else // some mode set
        {
            parameterInfo->mMode->setEnabled(true);
            parameterInfo->mInvert->setEnabled(true);
            parameterInfo->mValue->setEnabled(true);
        }
    }


    void GameControllerWindow::OnParameterModeComboBox(int value)
    {
        MCORE_UNUSED(value);

        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();
        if (activePreset == nullptr)
        {
            return;
        }

        MysticQt::ComboBox* combo = qobject_cast<MysticQt::ComboBox*>(sender());
        ParameterInfo* paramInfo = FindParamInfoByModeComboBox(combo);
        if (paramInfo == nullptr)
        {
            return;
        }

        EMotionFX::AnimGraphGameControllerSettings::ParameterInfo* settingsInfo = activePreset->FindParameterInfo(paramInfo->mAttributeSettings->GetName());
        MCORE_ASSERT(settingsInfo);
        settingsInfo->mMode = (EMotionFX::AnimGraphGameControllerSettings::ParameterMode)combo->currentIndex();
    }


    void GameControllerWindow::ReInitButtonInterface(uint32 buttonIndex)
    {
        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();
        if (activePreset == nullptr)
        {
            return;
        }

        EMotionFX::AnimGraphGameControllerSettings::ButtonInfo* settingsInfo = activePreset->FindButtonInfo(buttonIndex);
        MCORE_ASSERT(settingsInfo);

        // remove the old widget
        QLayoutItem* oldLayoutItem = mButtonGridLayout->itemAtPosition(buttonIndex, 2);
        if (oldLayoutItem)
        {
            QWidget* oldWidget = oldLayoutItem->widget();
            if (oldWidget)
            {
                oldWidget->hide();
                oldWidget->deleteLater();
            }
        }

        QWidget* widget = nullptr;
        switch (settingsInfo->mMode)
        {
        case EMotionFX::AnimGraphGameControllerSettings::BUTTONMODE_NONE:
            break;

        case EMotionFX::AnimGraphGameControllerSettings::BUTTONMODE_SWITCHSTATE:
        {
            widget = new QWidget();
            widget->setObjectName("GameControllerButtonModeSettings");
            widget->setStyleSheet("#GameControllerButtonModeSettings{ background-color: transparent; }");
            QHBoxLayout* layout = new QHBoxLayout();
            layout->setMargin(0);

            MysticQt::LinkWidget* linkWidget = new MysticQt::LinkWidget("Select node");
            linkWidget->setProperty("ButtonIndex", buttonIndex);
            if (settingsInfo->mString.empty() == false)
            {
                linkWidget->setText(settingsInfo->mString.c_str());
            }

            connect(linkWidget, SIGNAL(clicked()), this, SLOT(OnSelectNodeButtonClicked()));

            linkWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

            layout->addWidget(new QLabel("State:"));
            layout->addWidget(linkWidget);
            widget->setLayout(layout);
            break;
        }

        //case AnimGraphGameControllerSettings::BUTTONMODE_ENABLEBOOLFORONLYONEFRAMEONLY:
        //case AnimGraphGameControllerSettings::BUTTONMODE_TOGGLEBOOLEANPARAMETER:
        //case AnimGraphGameControllerSettings::BUTTONMODE_ENABLEBOOLWHILEPRESSED:
        //case AnimGraphGameControllerSettings::BUTTONMODE_DISABLEBOOLWHILEPRESSED:
        default:
        {
            widget = new QWidget();
            widget->setObjectName("GameControllerButtonModeSettings");
            widget->setStyleSheet("#GameControllerButtonModeSettings{ background-color: transparent; }");
            QHBoxLayout* layout = new QHBoxLayout();
            layout->setMargin(0);
            MysticQt::ComboBox* comboBox = new MysticQt::ComboBox();

            const uint32 numParameters = mAnimGraph->GetNumParameters();
            for (uint32 i = 0; i < numParameters; ++i)
            {
                if (mAnimGraph->GetParameter(i)->GetInterfaceType() == MCore::ATTRIBUTE_INTERFACETYPE_CHECKBOX)
                {
                    comboBox->addItem(mAnimGraph->GetParameter(i)->GetName());
                }
            }

            connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnButtonParameterComboBox(int)));
            comboBox->setProperty("ButtonIndex", buttonIndex);

            // select the correct parameter
            int comboIndex = comboBox->findText(settingsInfo->mString.c_str());
            if (comboIndex != -1)
            {
                comboBox->setCurrentIndex(comboIndex);
            }
            else
            {
                if (comboBox->count() > 0)
                {
                    //MCORE_ASSERT(false); // this shouldn't happen, please report Ben
                    //comboBox->setCurrentIndex( 0 );
                    //settingsInfo->mString = comboBox->itemText(0).toAscii().data();
                }
            }

            layout->addWidget(new QLabel("Bool Parameter:"));
            layout->addWidget(comboBox);
            widget->setLayout(layout);
            break;
        }
        }

        if (widget)
        {
            mButtonGridLayout->addWidget(widget, buttonIndex, 2);
        }
    }



    // open the node selection dialog for the node
    void GameControllerWindow::OnSelectNodeButtonClicked()
    {
        // get the sender widget
        MysticQt::LinkWidget* linkWidget = (MysticQt::LinkWidget*)(QWidget*)sender();
        if (linkWidget == nullptr)
        {
            return;
        }

        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();
        if (activePreset == nullptr)
        {
            return;
        }

        int32 buttonIndex = linkWidget->property("ButtonIndex").toInt();

        EMotionFX::AnimGraphGameControllerSettings::ButtonInfo* settingsInfo = activePreset->FindButtonInfo(buttonIndex);
        MCORE_ASSERT(settingsInfo);

        // create and show the state selection window
        BlendNodeSelectionWindow stateSelectionWindow(linkWidget, true, nullptr, EMotionFX::AnimGraphStateMachine::TYPE_ID);
        stateSelectionWindow.Update(mAnimGraph->GetID(), nullptr);
        stateSelectionWindow.setModal(true);
        if (stateSelectionWindow.exec() == QDialog::Rejected)   // we pressed cancel or the close cross
        {
            return;
        }

        // Get the selected states.
        const AZStd::vector<AnimGraphSelectionItem>& selectedStates = stateSelectionWindow.GetAnimGraphHierarchyWidget()->GetSelectedItems();
        if (selectedStates.empty())
        {
            return;
        }

        settingsInfo->mString = selectedStates[0].mNodeName.c_str();
        linkWidget->setText(selectedStates[0].mNodeName.c_str());
    }



    void GameControllerWindow::OnButtonParameterComboBox(int value)
    {
        MCORE_UNUSED(value);

        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();
        if (activePreset == nullptr)
        {
            return;
        }

        MysticQt::ComboBox* combo = qobject_cast<MysticQt::ComboBox*>(sender());
        int32 buttonIndex = combo->property("ButtonIndex").toInt();

        EMotionFX::AnimGraphGameControllerSettings::ButtonInfo* settingsInfo = activePreset->FindButtonInfo(buttonIndex);
        MCORE_ASSERT(settingsInfo);

        const uint32 parameterIndex = mAnimGraph->FindParameterIndex(FromQtString(combo->currentText()).c_str());
        if (parameterIndex != MCORE_INVALIDINDEX32)
        {
            settingsInfo->mString = mAnimGraph->GetParameter(parameterIndex)->GetName();
        }
        else
        {
            settingsInfo->mString.clear();
        }

        // update the parameter window
        mPlugin->GetParameterWindow()->Init();
    }



    void GameControllerWindow::OnButtonModeComboBox(int value)
    {
        MCORE_UNUSED(value);

        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();
        if (activePreset == nullptr)
        {
            return;
        }

        MysticQt::ComboBox* combo = qobject_cast<MysticQt::ComboBox*>(sender());
        ButtonInfo* buttonInfo = FindButtonInfo(combo);
        if (buttonInfo == nullptr)
        {
            return;
        }

        EMotionFX::AnimGraphGameControllerSettings::ButtonInfo* settingsInfo = activePreset->FindButtonInfo(buttonInfo->mButtonIndex);
        MCORE_ASSERT(settingsInfo);
        settingsInfo->mMode = (EMotionFX::AnimGraphGameControllerSettings::ButtonMode)combo->currentIndex();

        // check if the button info is pointing to a correct parameter
        if (mAnimGraph->FindParameter(settingsInfo->mString.c_str()) == nullptr)
        {
            // find the first bool parameter
            const uint32 numParameters = mAnimGraph->GetNumParameters();
            for (uint32 i = 0; i < numParameters; ++i)
            {
                if (mAnimGraph->GetParameter(i)->GetInterfaceType() == MCore::ATTRIBUTE_INTERFACETYPE_CHECKBOX)
                {
                    settingsInfo->mString = mAnimGraph->GetParameter(i)->GetName();
                    break;
                }
            }
        }

        ReInitButtonInterface(buttonInfo->mButtonIndex);

        // update the parameter window
        mPlugin->GetParameterWindow()->Init();
    }


    void GameControllerWindow::OnAddPresetButton()
    {
        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        uint32 presetNumber = gameControllerSettings->GetNumPresets();
        mString = AZStd::string::format("Preset %d", presetNumber);
        while (gameControllerSettings->FindPresetIndexByName(mString.c_str()) != MCORE_INVALIDINDEX32)
        {
            presetNumber++;
            mString = AZStd::string::format("Preset %d", presetNumber);
        }

        EMotionFX::AnimGraphGameControllerSettings::Preset* preset = EMotionFX::AnimGraphGameControllerSettings::Preset::Create(mString.c_str());
        gameControllerSettings->AddPreset(preset);

        ReInit();
    }


    void GameControllerWindow::OnPresetComboBox(int value)
    {
        MCORE_UNUSED(value);

        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        MysticQt::ComboBox* combo = qobject_cast<MysticQt::ComboBox*>(sender());
        EMotionFX::AnimGraphGameControllerSettings::Preset* preset = gameControllerSettings->GetPreset(combo->currentIndex());
        gameControllerSettings->SetActivePreset(preset);

        ReInit();
    }


    void GameControllerWindow::OnRemovePresetButton()
    {
        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        uint32 presetIndex = mPresetComboBox->currentIndex();
        gameControllerSettings->RemovePreset(presetIndex);

        EMotionFX::AnimGraphGameControllerSettings::Preset* preset = nullptr;
        if (gameControllerSettings->GetNumPresets() > 0)
        {
            if (presetIndex >= gameControllerSettings->GetNumPresets())
            {
                preset = gameControllerSettings->GetPreset(gameControllerSettings->GetNumPresets() - 1);
            }
            else
            {
                preset = gameControllerSettings->GetPreset(presetIndex);
            }
        }

        gameControllerSettings->SetActivePreset(preset);

        ReInit();
    }


    void GameControllerWindow::OnPresetNameChanged()
    {
        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        assert(sender()->inherits("QLineEdit"));
        QLineEdit* widget = qobject_cast<QLineEdit*>(sender());
        AZStd::string newValue;
        FromQtString(widget->text(), &newValue);

        // get the currently selected preset
        uint32 presetIndex = mPresetComboBox->currentIndex();

        uint32 newValueIndex = gameControllerSettings->FindPresetIndexByName(newValue.c_str());
        if (newValueIndex == MCORE_INVALIDINDEX32)
        {
            EMotionFX::AnimGraphGameControllerSettings::Preset* preset = gameControllerSettings->GetPreset(presetIndex);
            preset->SetName(newValue.c_str());
            ReInit();
        }
    }


    void GameControllerWindow::OnPresetNameEdited(const QString& text)
    {
        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // check if there already is a preset with the currently entered name
        uint32 presetIndex = gameControllerSettings->FindPresetIndexByName(FromQtString(text).c_str());
        if (presetIndex != MCORE_INVALIDINDEX32 && presetIndex != gameControllerSettings->GetActivePresetIndex())
        {
            GetManager()->SetWidgetAsInvalidInput(mPresetNameLineEdit);
        }
        else
        {
            mPresetNameLineEdit->setStyleSheet("");
        }
    }


    GameControllerWindow::ParameterInfo* GameControllerWindow::FindParamInfoByAxisComboBox(MysticQt::ComboBox* comboBox)
    {
        // get the number of parameter infos and iterate through them
        const uint32 numParamInfos = mParameterInfos.GetLength();
        for (uint32 i = 0; i < numParamInfos; ++i)
        {
            if (mParameterInfos[i].mAxis == comboBox)
            {
                return &mParameterInfos[i];
            }
        }

        // return failure
        return nullptr;
    }


    void GameControllerWindow::OnAxisComboBox(int value)
    {
        MCORE_UNUSED(value);

        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();
        if (activePreset == nullptr)
        {
            return;
        }

        MysticQt::ComboBox* combo = qobject_cast<MysticQt::ComboBox*>(sender());
        ParameterInfo* paramInfo = FindParamInfoByAxisComboBox(combo);
        if (paramInfo == nullptr)
        {
            return;
        }

        EMotionFX::AnimGraphGameControllerSettings::ParameterInfo* settingsInfo = activePreset->FindParameterInfo(paramInfo->mAttributeSettings->GetName());
        MCORE_ASSERT(settingsInfo);

    #ifdef HAS_GAME_CONTROLLER
        if (paramInfo->mAttributeSettings->GetDefaultValue()->GetType() == MCore::AttributeFloat::TYPE_ID)
        {
            const uint32 elementID = (GameController::Axis)mGameController->FindElemendIDByName(FromQtString(combo->currentText()).c_str());
            if (elementID >= MCORE_INVALIDINDEX8)
            {
                settingsInfo->mAxis = MCORE_INVALIDINDEX8;
            }
            else
            {
                settingsInfo->mAxis = elementID;
            }
        }
        else
        if (paramInfo->mAttributeSettings->GetDefaultValue()->GetType() == MCore::AttributeVector2::TYPE_ID)
        {
            if (value == 0)
            {
                settingsInfo->mAxis = MCORE_INVALIDINDEX8;
            }
            else
            {
                settingsInfo->mAxis = value - 1;
            }
        }
    #else
        settingsInfo->mAxis = MCORE_INVALIDINDEX8;
    #endif

        // update the interface
        UpdateParameterInterface(paramInfo);

        // update the parameter window
        mPlugin->GetParameterWindow()->Init();
    }


    GameControllerWindow::ParameterInfo* GameControllerWindow::FindParamInfoByCheckBox(QCheckBox* checkBox)
    {
        // get the number of parameter infos and iterate through them
        const uint32 numParamInfos = mParameterInfos.GetLength();
        for (uint32 i = 0; i < numParamInfos; ++i)
        {
            if (mParameterInfos[i].mInvert == checkBox)
            {
                return &mParameterInfos[i];
            }
        }

        // return failure
        return nullptr;
    }


    void GameControllerWindow::OnInvertCheckBoxChanged(int state)
    {
        MCORE_UNUSED(state);

        // get the game controller settings from the current anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();
        if (activePreset == nullptr)
        {
            return;
        }

        QCheckBox* checkBox = qobject_cast<QCheckBox*>(sender());
        ParameterInfo* paramInfo = FindParamInfoByCheckBox(checkBox);
        if (paramInfo == nullptr)
        {
            return;
        }

        EMotionFX::AnimGraphGameControllerSettings::ParameterInfo* settingsInfo = activePreset->FindParameterInfo(paramInfo->mAttributeSettings->GetName());
        MCORE_ASSERT(settingsInfo);
        settingsInfo->mInvert = checkBox->checkState() == Qt::Checked ? true : false;
    }


    // new hardware got detected, reinit direct input
    void GameControllerWindow::HardwareChangeDetected()
    {
        // in case there is no controller plugged in watch out for a new one
        InitGameController();
        UpdateGameControllerComboBox();
        AutoSelectGameController();
        ReInit();
        mPlugin->GetParameterWindow()->Init();
    }


    // handle timer event
    void GameControllerWindow::timerEvent(QTimerEvent* event)
    {
    #ifndef HAS_GAME_CONTROLLER
        MCORE_UNUSED(event);
    #endif

        if (EMotionFX::GetRecorder().GetIsInPlayMode() && EMotionFX::GetRecorder().GetRecordTime() > MCore::Math::epsilon)
        {
            return;
        }

        // update the game controller
    #ifdef HAS_GAME_CONTROLLER
        mGameController->Update();

        // check if the game controller is usable and if we have actually checked it in the combobox, if not return directly
        if (mGameController->GetIsValid() == false || mGameControllerComboBox->currentIndex() == 0)
        {
            return;
        }
    #else
        return;
    #endif

    #ifdef HAS_GAME_CONTROLLER
        // get the selected actor instance
        EMotionFX::ActorInstance* actorInstance = GetCommandManager()->GetCurrentSelection().GetSingleActorInstance();
        if (actorInstance == nullptr)
        {
            return;
        }

        // get the anim graph instance for the selected actor instance
        EMotionFX::AnimGraphInstance* animGraphInstance = nullptr;
        animGraphInstance = actorInstance->GetAnimGraphInstance();
        if (animGraphInstance)
        {
            if (animGraphInstance->GetAnimGraph() != mAnimGraph) // if the selected anim graph instance isn't equal to the one of the actor instance
            {
                return;
            }
        }
        else
        {
            return;
        }

        // get the game controller settings from the anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = mAnimGraph->GetGameControllerSettings();

        // get the active preset
        EMotionFX::AnimGraphGameControllerSettings::Preset* activePreset = gameControllerSettings->GetActivePreset();
        if (activePreset == nullptr)
        {
            return;
        }

        const float timeDelta = mDeltaTimer.StampAndGetDeltaTimeInSeconds();

        // get the number of parameters and iterate through them
        uint32 i;
        const uint32 numParameters = mAnimGraph->GetNumParameters();
        for (i = 0; i < numParameters; ++i)
        {
            // get the attribute settings
            MCore::AttributeSettings* attributeSettings = mAnimGraph->GetParameter(i);

            // get the game controller settings info for the given parameter
            EMotionFX::AnimGraphGameControllerSettings::ParameterInfo* settingsInfo = activePreset->FindParameterInfo(attributeSettings->GetName());
            MCORE_ASSERT(settingsInfo);

            // skip all parameters whose axis is set to None
            if (settingsInfo->mAxis == MCORE_INVALIDINDEX8)
            {
                continue;
            }

            // find the corresponding attribute
            MCore::Attribute* attribute = animGraphInstance->GetParameterValue(i);

            if (attribute->GetType() == MCore::AttributeFloat::TYPE_ID)
            {
                // get the current value from the game controller
                float value = mGameController->GetValue(settingsInfo->mAxis);
                const float minValue = static_cast<MCore::AttributeFloat*>(attributeSettings->GetMinValue())->GetValue();
                const float maxValue = static_cast<MCore::AttributeFloat*>(attributeSettings->GetMaxValue())->GetValue();

                switch (settingsInfo->mMode)
                {
                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_STANDARD:
                {
                    if (settingsInfo->mInvert)
                    {
                        value = -value;
                    }

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_ZEROTOONE:
                {
                    const float normalizedValue = (value + 1.0) * 0.5f;
                    value = normalizedValue;

                    if (settingsInfo->mInvert)
                    {
                        value = 1.0f - value;
                    }

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_PARAMRANGE:
                {
                    float normalizedValue = (value + 1.0) * 0.5f;
                    if (settingsInfo->mInvert)
                    {
                        normalizedValue = 1.0f - normalizedValue;
                    }

                    value = minValue + normalizedValue * (maxValue - minValue);

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_POSITIVETOPARAMRANGE:
                {
                    if (value < 0.0f)
                    {
                        break;
                    }

                    if (settingsInfo->mInvert)
                    {
                        value = -value;
                    }
                    value = minValue + value * (maxValue - minValue);

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_NEGATIVETOPARAMRANGE:
                {
                    if (value > 0.0f)
                    {
                        break;
                    }

                    if (settingsInfo->mInvert)
                    {
                        value = -value;
                    }
                    value = minValue + value * (maxValue - minValue);

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_ROTATE_CHARACTER:
                {
                    if (settingsInfo->mInvert)
                    {
                        value = -value;
                    }

                    if (value > 0.1f || value < -0.1f)
                    {
                        // only process in case the parameter info is enabled
                        if (settingsInfo->mEnabled)
                        {
                            MCore::Quaternion localRot = actorInstance->GetLocalRotation();
                            localRot = localRot * MCore::Quaternion(AZ::Vector3(0.0f, 0.0f, 1.0f), value * timeDelta * 3.0f);
                            actorInstance->SetLocalRotation(localRot);
                        }
                    }

                    break;
                }
                }

                // set the value to the attribute in case the parameter info is enabled
                if (settingsInfo->mEnabled)
                {
                    static_cast<MCore::AttributeFloat*>(attribute)->SetValue(value);
                }

                // check if we also need to update the attribute widget in the parameter window
                if (event->timerId() == mInterfaceTimerID)
                {
                    // find the corresponding attribute widget and set the value in case the parameter info is enabled
                    if (settingsInfo->mEnabled)
                    {
                        MysticQt::AttributeWidget* attributeWidget = mPlugin->GetParameterWindow()->FindAttributeWidget(attributeSettings);
                        if (attributeWidget)
                        {
                            attributeWidget->SetValue(attribute);
                        }
                    }

                    // also update the preview value in the game controller window
                    ParameterInfo* interfaceParamInfo = FindButtonInfoByAttributeInfo(attributeSettings);
                    if (interfaceParamInfo)
                    {
                        mString = AZStd::string::format("%.2f", value);
                        interfaceParamInfo->mValue->setText(mString.c_str());
                    }
                }
            } // if it's a float attribute
            else
            if (attribute->GetType() == MCore::AttributeVector2::TYPE_ID)
            {
                // get the current value from the game controller
                AZ::Vector2 value(0.0f, 0.0f);
                if (settingsInfo->mAxis == 0)
                {
                    value.SetX(mGameController->GetValue(GameController::ELEM_POS_X));
                    value.SetY(mGameController->GetValue(GameController::ELEM_POS_Y));
                }
                else
                {
                    value.SetX(mGameController->GetValue(GameController::ELEM_ROT_X));
                    value.SetY(mGameController->GetValue(GameController::ELEM_ROT_Y));
                }

                const AZ::Vector2 minValue = static_cast<MCore::AttributeVector2*>(attributeSettings->GetMinValue())->GetValue();
                const AZ::Vector2 maxValue = static_cast<MCore::AttributeVector2*>(attributeSettings->GetMaxValue())->GetValue();

                switch (settingsInfo->mMode)
                {
                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_STANDARD:
                {
                    if (settingsInfo->mInvert)
                    {
                        value = -value;
                    }

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_ZEROTOONE:
                {
                    const float normalizedValueX = (value.GetX() + 1.0) * 0.5f;
                    value.SetX(normalizedValueX);

                    const float normalizedValueY = (value.GetY() + 1.0) * 0.5f;
                    value.SetY(normalizedValueY);

                    if (settingsInfo->mInvert)
                    {
                        value.SetX(1.0f - value.GetX());
                        value.SetY(1.0f - value.GetY());
                    }

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_PARAMRANGE:
                {
                    float normalizedValueX = (value.GetX() + 1.0) * 0.5f;
                    float normalizedValueY = (value.GetY() + 1.0) * 0.5f;
                    if (settingsInfo->mInvert)
                    {
                        normalizedValueX = 1.0f - normalizedValueX;
                        normalizedValueY = 1.0f - normalizedValueY;
                    }

                    value.SetX(minValue.GetX() + normalizedValueX * (maxValue.GetX() - minValue.GetX()));
                    value.SetY(minValue.GetY() + normalizedValueY * (maxValue.GetY() - minValue.GetY()));

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_POSITIVETOPARAMRANGE:
                {
                    if (value.GetX() > 0.0f)
                    {
                        if (settingsInfo->mInvert)
                        {
                            value.SetX(-value.GetX());
                        }
                        value.SetX(minValue.GetX() + value.GetX() * (maxValue.GetX() - minValue.GetX()));
                    }

                    if (value.GetY() > 0.0f)
                    {
                        if (settingsInfo->mInvert)
                        {
                            value.SetY(-value.GetY());
                        }
                        value.SetY(minValue.GetY() + value.GetY() * (maxValue.GetY() - minValue.GetY()));
                    }

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_NEGATIVETOPARAMRANGE:
                {
                    if (value.GetX() < 0.0f)
                    {
                        if (settingsInfo->mInvert)
                        {
                            value.SetX(-value.GetX());
                        }
                        value.SetX(minValue.GetX() + value.GetX() * (maxValue.GetX() - minValue.GetX()));
                    }

                    if (value.GetY() < 0.0f)
                    {
                        if (settingsInfo->mInvert)
                        {
                            value.SetY(-value.GetY());
                        }
                        value.SetY(minValue.GetY() + value.GetY() * (maxValue.GetY() - minValue.GetY()));
                    }

                    break;
                }

                case EMotionFX::AnimGraphGameControllerSettings::PARAMMODE_ROTATE_CHARACTER:
                {
                    if (settingsInfo->mInvert)
                    {
                        value = -value;
                    }

                    if (value.GetX() > 0.1f || value.GetX() < -0.1f)
                    {
                        // only process in case the parameter info is enabled
                        if (settingsInfo->mEnabled)
                        {
                            MCore::Quaternion localRot = actorInstance->GetLocalRotation();
                            localRot = localRot * MCore::Quaternion(AZ::Vector3(0.0f, 0.0f, 1.0f), value.GetX() * timeDelta * 3.0f);
                            actorInstance->SetLocalRotation(localRot);
                        }
                    }

                    break;
                }
                default:
                    MCORE_ASSERT(false);
                }

                // set the value to the attribute in case the parameter info is enabled
                if (settingsInfo->mEnabled)
                {
                    static_cast<MCore::AttributeVector2*>(attribute)->SetValue(value);
                }

                // check if we also need to update the attribute widget in the parameter window
                if (event->timerId() == mInterfaceTimerID)
                {
                    // find the corresponding attribute widget and set the value in case the parameter info is enabled
                    if (settingsInfo->mEnabled)
                    {
                        MysticQt::AttributeWidget* attributeWidget = mPlugin->GetParameterWindow()->FindAttributeWidget(attributeSettings);
                        if (attributeWidget)
                        {
                            attributeWidget->SetValue(attribute);
                        }
                    }

                    // also update the preview value in the game controller window
                    ParameterInfo* interfaceParamInfo = FindButtonInfoByAttributeInfo(attributeSettings);
                    if (interfaceParamInfo)
                    {
                        mString = AZStd::string::format("%.2f, %.2f", value.GetX(), value.GetY());
                        interfaceParamInfo->mValue->setText(mString.c_str());
                    }
                }
            } // if it's a vector2 attribute
        } // for all parameters

        // update the buttons
        const uint32 numButtons = mGameController->GetNumButtons();
        for (i = 0; i < numButtons; ++i)
        {
            bool isPressed = mGameController->GetIsButtonPressed(i);

            // get the game controller settings info for the given button
            EMotionFX::AnimGraphGameControllerSettings::ButtonInfo* settingsInfo = activePreset->FindButtonInfo(i);
            MCORE_ASSERT(settingsInfo);

            if (settingsInfo->mString.empty())
            {
                continue;
            }

            // skip this button in case control is disabled
            if (settingsInfo->mEnabled == false)
            {
                continue;
            }

            switch (settingsInfo->mMode)
            {
            case EMotionFX::AnimGraphGameControllerSettings::BUTTONMODE_NONE:
                break;

            case EMotionFX::AnimGraphGameControllerSettings::BUTTONMODE_SWITCHSTATE:
            {
                if (isPressed)
                {
                    // switch to the desired state
                    if (animGraphInstance)
                    {
                        animGraphInstance->TransitionToState(settingsInfo->mString.c_str());
                    }
                }
                break;
            }

            case EMotionFX::AnimGraphGameControllerSettings::BUTTONMODE_TOGGLEBOOLEANPARAMETER:
            {
                // find the corresponding attribute
                const uint32 parameterIndex = mAnimGraph->FindParameterIndex(settingsInfo->mString.c_str());
                if (parameterIndex == MCORE_INVALIDINDEX32)
                {
                    continue;
                }

                MCore::Attribute* attribute = animGraphInstance->GetParameterValue(parameterIndex);

                const bool oldValue = static_cast<MCore::AttributeFloat*>(attribute)->GetValue();

                if (isPressed && settingsInfo->mOldIsPressed == false)
                {
                    static_cast<MCore::AttributeFloat*>(attribute)->SetValue((!oldValue) ? 1.0f : 0.0f);
                }

                // check if we also need to update the attribute widget in the parameter window
                if (event->timerId() == mInterfaceTimerID)
                {
                    // get the attribute settings
                    MCore::AttributeSettings* attributeSettings = mAnimGraph->GetParameter(parameterIndex);

                    // find the corresponding attribute widget
                    MysticQt::AttributeWidget* attributeWidget = mPlugin->GetParameterWindow()->FindAttributeWidget(attributeSettings);
                    if (attributeWidget)
                    {
                        attributeWidget->SetValue(attribute);
                    }
                }

                break;
            }

            case EMotionFX::AnimGraphGameControllerSettings::BUTTONMODE_ENABLEBOOLWHILEPRESSED:
            {
                // find the corresponding attribute
                const uint32 parameterIndex = mAnimGraph->FindParameterIndex(settingsInfo->mString.c_str());
                if (parameterIndex == MCORE_INVALIDINDEX32)
                {
                    continue;
                }

                MCore::Attribute* attribute = animGraphInstance->GetParameterValue(parameterIndex);

                static_cast<MCore::AttributeFloat*>(attribute)->SetValue(isPressed ? 1.0f : 0.0f);

                // check if we also need to update the attribute widget in the parameter window
                if (event->timerId() == mInterfaceTimerID)
                {
                    // get the attribute settings
                    MCore::AttributeSettings* attributeSettings = mAnimGraph->GetParameter(parameterIndex);

                    // find the corresponding attribute widget
                    MysticQt::AttributeWidget* attributeWidget = mPlugin->GetParameterWindow()->FindAttributeWidget(attributeSettings);
                    if (attributeWidget)
                    {
                        attributeWidget->SetValue(attribute);
                    }
                }

                break;
            }

            case EMotionFX::AnimGraphGameControllerSettings::BUTTONMODE_DISABLEBOOLWHILEPRESSED:
            {
                // find the corresponding attribute
                const uint32 parameterIndex = mAnimGraph->FindParameterIndex(settingsInfo->mString.c_str());
                if (parameterIndex == MCORE_INVALIDINDEX32)
                {
                    continue;
                }

                MCore::Attribute* attribute = animGraphInstance->GetParameterValue(parameterIndex);
                static_cast<MCore::AttributeFloat*>(attribute)->SetValue((!isPressed) ? 1.0f : 0.0f);

                // check if we also need to update the attribute widget in the parameter window
                if (event->timerId() == mInterfaceTimerID)
                {
                    // get the attribute settings
                    MCore::AttributeSettings* attributeSettings = mAnimGraph->GetParameter(parameterIndex);

                    // find the corresponding attribute widget
                    MysticQt::AttributeWidget* attributeWidget = mPlugin->GetParameterWindow()->FindAttributeWidget(attributeSettings);
                    if (attributeWidget)
                    {
                        attributeWidget->SetValue(attribute);
                    }
                }

                break;
            }

            case EMotionFX::AnimGraphGameControllerSettings::BUTTONMODE_ENABLEBOOLFORONLYONEFRAMEONLY:
            {
                // find the corresponding attribute
                const uint32 parameterIndex = mAnimGraph->FindParameterIndex(settingsInfo->mString.c_str());
                if (parameterIndex == MCORE_INVALIDINDEX32)
                {
                    continue;
                }

                MCore::Attribute* attribute = animGraphInstance->GetParameterValue(parameterIndex);

                // in case the button got pressed and we are allowed to set it to true, do that for only one frame
                static bool isAllowed = true;
                if (isPressed && isAllowed)
                {
                    // set the bool parameter to true this time
                    static_cast<MCore::AttributeFloat*>(attribute)->SetValue(1.0f);

                    // don't allow to set the boolean parameter to true next frame
                    isAllowed = false;
                }
                else
                {
                    // disable the boolean parameter as either the button is not pressed or we are not allowed to enable it as that single frame tick already happened
                    static_cast<MCore::AttributeFloat*>(attribute)->SetValue(0.0f);

                    // allow it again as soon as the user left the button
                    if (isPressed == false)
                    {
                        isAllowed = true;
                    }
                }

                // check if we also need to update the attribute widget in the parameter window
                if (event->timerId() == mInterfaceTimerID)
                {
                    // get the attribute settings
                    MCore::AttributeSettings* attributeSettings = mAnimGraph->GetParameter(parameterIndex);

                    // find the corresponding attribute widget
                    MysticQt::AttributeWidget* attributeWidget = mPlugin->GetParameterWindow()->FindAttributeWidget(attributeSettings);
                    if (attributeWidget)
                    {
                        attributeWidget->SetValue(attribute);
                    }
                }

                break;
            }
            }

            // store the information about the button press for the next frame
            settingsInfo->mOldIsPressed = isPressed;
        }

        // check if the interface timer is ticking
        if (event->timerId() == mInterfaceTimerID)
        {
            // update the interface elements
            for (i = 0; i < GameController::NUM_ELEMENTS; ++i)
            {
                if (mGameController->GetIsPresent(i))
                {
                    const float value = mGameController->GetValue(i);
                    if (value > 1000.0f)
                    {
                        mString.clear();
                    }
                    else
                    {
                        mString = AZStd::string::format("%.2f", value);
                    }

                    mPreviewLabels[i]->setText(mString.c_str());
                }
            }

            // update the active button string
            mString.clear();
            for (i = 0; i < numButtons; ++i)
            {
                if (mGameController->GetIsButtonPressed(i))
                {
                    mString += AZStd::string::format("%s%d ", (i < 10) ? "0" : "", i);
                }
            }
            if (mString.size() == 0)
            {
                mPreviewLabels[GameController::NUM_ELEMENTS]->setText(" ");
            }
            else
            {
                mPreviewLabels[GameController::NUM_ELEMENTS]->setText(mString.c_str());
            }
        }
    #endif
    }


    //----------------------------------------------------------------------------------------------------------------------------------
    // CreateParameter callback
    //----------------------------------------------------------------------------------------------------------------------------------
    void ReInitGameControllerWindow()
    {
        // get the plugin object
        EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(AnimGraphPlugin::CLASS_ID);
        if (plugin == nullptr)
        {
            return;
        }

        // re-init the param window
    #ifdef HAS_GAME_CONTROLLER
        AnimGraphPlugin* animGraphPlugin = (AnimGraphPlugin*)plugin;
        animGraphPlugin->GetGameControllerWindow()->ReInit();
    #endif
    }


    bool GameControllerWindow::CommandCreateBlendParameterCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)     { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); ReInitGameControllerWindow(); return true; }
    bool GameControllerWindow::CommandCreateBlendParameterCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)        { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); ReInitGameControllerWindow(); return true; }
    bool GameControllerWindow::CommandRemoveBlendParameterCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)     { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); ReInitGameControllerWindow(); return true; }
    bool GameControllerWindow::CommandRemoveBlendParameterCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)        { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); ReInitGameControllerWindow(); return true; }
    bool GameControllerWindow::CommandSelectCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        if (CommandSystem::CheckIfHasAnimGraphSelectionParameter(commandLine) == false)
        {
            return true;
        }
        ReInitGameControllerWindow();
        return true;
    }
    bool GameControllerWindow::CommandSelectCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        if (CommandSystem::CheckIfHasAnimGraphSelectionParameter(commandLine) == false)
        {
            return true;
        }
        ReInitGameControllerWindow();
        return true;
    }
    bool GameControllerWindow::CommandUnselectCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        if (CommandSystem::CheckIfHasAnimGraphSelectionParameter(commandLine) == false)
        {
            return true;
        }
        ReInitGameControllerWindow();
        return true;
    }
    bool GameControllerWindow::CommandUnselectCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        if (CommandSystem::CheckIfHasAnimGraphSelectionParameter(commandLine) == false)
        {
            return true;
        }
        ReInitGameControllerWindow();
        return true;
    }
    bool GameControllerWindow::CommandClearSelectionCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)           { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); ReInitGameControllerWindow(); return true; }
    bool GameControllerWindow::CommandClearSelectionCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)              { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); ReInitGameControllerWindow(); return true; }

    bool GameControllerWindow::CommandAdjustBlendParameterCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        // get the plugin object
        EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(AnimGraphPlugin::CLASS_ID);
        //  AnimGraphPlugin* animGraphPlugin = (AnimGraphPlugin*)plugin;
        if (plugin == nullptr)
        {
            return false;
        }

        uint32                  animGraphID    = commandLine.GetValueAsInt("animGraphID", command);
        EMotionFX::AnimGraph*  animGraph      = EMotionFX::GetAnimGraphManager().FindAnimGraphByID(animGraphID);

        if (animGraph == nullptr)
        {
            MCore::LogError("Cannot adjust parameter to anim graph. Anim graph id '%i' is not valid.", animGraphID);
            return false;
        }

        // get the game controller settings from the anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = animGraph->GetGameControllerSettings();

        AZStd::string name;
        commandLine.GetValue("name", command, &name);

        AZStd::string newName;
        commandLine.GetValue("newName", command, &newName);

        if (gameControllerSettings)
        {
            gameControllerSettings->OnParameterNameChange(name.c_str(), newName.c_str());
        }

        ReInitGameControllerWindow();
        return true;
    }


    bool GameControllerWindow::CommandAdjustBlendParameterCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        // get the plugin object
        EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(AnimGraphPlugin::CLASS_ID);
        //AnimGraphPlugin* animGraphPlugin = (AnimGraphPlugin*)plugin;
        if (plugin == nullptr)
        {
            return false;
        }

        const uint32                animGraphID    = commandLine.GetValueAsInt("animGraphID", command);
        EMotionFX::AnimGraph*      animGraph      = EMotionFX::GetAnimGraphManager().FindAnimGraphByID(animGraphID);
        if (animGraph == nullptr)
        {
            MCore::LogError("Cannot adjust parameter to anim graph. Anim graph id '%i' is not valid.", animGraphID);
            return false;
        }

        // get the game controller settings from the anim graph
        EMotionFX::AnimGraphGameControllerSettings* gameControllerSettings = animGraph->GetGameControllerSettings();

        AZStd::string name;
        commandLine.GetValue("name", command, &name);

        AZStd::string newName;
        commandLine.GetValue("newName", command, &newName);

        if (gameControllerSettings)
        {
            gameControllerSettings->OnParameterNameChange(newName.c_str(), name.c_str());
        }

        ReInitGameControllerWindow();
        return true;
    }
} // namespace EMStudio

#include <EMotionFX/Tools/EMotionStudio/Plugins/StandardPlugins/Source/AnimGraph/GameControllerWindow.moc>

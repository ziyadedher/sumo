/****************************************************************************/
/// @file    GNEPolygonFrame.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Aug 2017
/// @version $Id: GNEPolygonFrame.cpp 25295 2017-07-22 17:55:46Z behrisch $
///
// The Widget for add polygons
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.dlr.de/
// Copyright (C) 2001-2017 DLR (http://www.dlr.de/) and contributors
/****************************************************************************/
//
//   This file is part of SUMO.
//   SUMO is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/

// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <iostream>
#include <utils/foxtools/fxexdefs.h>
#include <utils/foxtools/MFXUtils.h>
#include <utils/common/MsgHandler.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/div/GUIIOGlobals.h>
#include <utils/gui/div/GUIGlobalSelection.h>
#include <utils/gui/div/GUIDesigns.h>
#include <utils/gui/globjects/GUIGlObjectStorage.h>
#include <utils/gui/images/GUIIconSubSys.h>

#include "GNEPolygonFrame.h"
#include "GNEViewNet.h"
#include "GNEViewParent.h"
#include "GNENet.h"
#include "GNEUndoList.h"
#include "GNEChange_Selection.h"
#include "GNEAttributeCarrier.h"
#include "GNEPoly.h"
#include "GNEPOI.h"



// ===========================================================================
// FOX callback mapping
// ===========================================================================

FXDEFMAP(GNEPolygonFrame) GNEShapeMap[] = {
    FXMAPFUNC(SEL_COMMAND, MID_GNE_MODE_ADDITIONAL_ITEM, GNEPolygonFrame::onCmdSelectShape),
};

FXDEFMAP(GNEPolygonFrame::ShapeAttributeSingle) GNEsingleShapeParameterMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_GNE_MODE_ADDITIONAL_CHANGEPARAMETER_TEXT,   GNEPolygonFrame::ShapeAttributeSingle::onCmdSetAttribute),
    FXMAPFUNC(SEL_COMMAND,  MID_GNE_MODE_ADDITIONAL_CHANGEPARAMETER_BOOL,   GNEPolygonFrame::ShapeAttributeSingle::onCmdSetBooleanAttribute),
};

FXDEFMAP(GNEPolygonFrame::ShapeAttributes) GNEadditionalParametersMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_HELP,   GNEPolygonFrame::ShapeAttributes::onCmdHelp),
};

FXDEFMAP(GNEPolygonFrame::NeteditAttributes) GNEEditorParametersMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_GNE_SET_BLOCKING_MOVEMENT,  GNEPolygonFrame::NeteditAttributes::onCmdSetBlockMovement),
    FXMAPFUNC(SEL_COMMAND,  MID_GNE_SET_BLOCKING_SHAPE,     GNEPolygonFrame::NeteditAttributes::onCmdSetBlockShape),
};

// Object implementation
FXIMPLEMENT(GNEPolygonFrame,                        FXVerticalFrame,    GNEShapeMap,                    ARRAYNUMBER(GNEShapeMap))
FXIMPLEMENT(GNEPolygonFrame::ShapeAttributeSingle,  FXHorizontalFrame,  GNEsingleShapeParameterMap,     ARRAYNUMBER(GNEsingleShapeParameterMap))
FXIMPLEMENT(GNEPolygonFrame::ShapeAttributes,       FXGroupBox,         GNEadditionalParametersMap,     ARRAYNUMBER(GNEadditionalParametersMap))
FXIMPLEMENT(GNEPolygonFrame::NeteditAttributes,     FXGroupBox,         GNEEditorParametersMap,         ARRAYNUMBER(GNEEditorParametersMap))

// ===========================================================================
// method definitions
// ===========================================================================

GNEPolygonFrame::GNEPolygonFrame(FXHorizontalFrame* horizontalFrameParent, GNEViewNet* viewNet):
    GNEFrame(horizontalFrameParent, viewNet, "Shapes"),
    myActualShapeType(SUMO_TAG_NOTHING) {

    // Create groupBox for myShapeMatchBox
    myGroupBoxForMyShapeMatchBox = new FXGroupBox(myContentFrame, "Shape element", GUIDesignGroupBoxFrame);

    // Create FXListBox in myGroupBoxForMyShapeMatchBox
    myShapeMatchBox = new FXComboBox(myGroupBoxForMyShapeMatchBox, GUIDesignComboBoxNCol, this, MID_GNE_MODE_ADDITIONAL_ITEM, GUIDesignComboBox);

    // Create additional parameters
    myadditionalParameters = new GNEPolygonFrame::ShapeAttributes(myViewNet, myContentFrame);

    // Create Netedit parameter
    myEditorParameters = new GNEPolygonFrame::NeteditAttributes(myContentFrame);

    // Add options to myShapeMatchBox
    for (auto i : GNEAttributeCarrier::allowedShapeTags()) {
        myShapeMatchBox->appendItem(toString(i).c_str());
    }

    // Set visible items
    myShapeMatchBox->setNumVisible((int)myShapeMatchBox->getNumItems());

    // If there are additionals
    if (GNEAttributeCarrier::allowedShapeTags().size() > 0) {
        // Set myActualShapeType and show
        myActualShapeType = GNEAttributeCarrier::allowedShapeTags().front();
        setParametersOfShape(myActualShapeType);
    }
}


GNEPolygonFrame::~GNEPolygonFrame() {
    gSelected.remove2Update();
}


GNEPolygonFrame::AddShapeResult
GNEPolygonFrame::addShape(GNENetElement* netElement, GUISUMOAbstractView* parent) {
    // check if current selected additional is valid
    if (myActualShapeType == SUMO_TAG_NOTHING) {
        myViewNet->setStatusBarText("Current selected shape isn't valid.");
        return ADDSHAPE_INVALID_ARGUMENTS;
    }

    // Declare map to keep values
    std::map<SumoXMLAttr, std::string> valuesOfElement = myadditionalParameters->getAttributesAndValues();

    // limit position depending if show grid is enabled
    Position currentPosition = parent->snapToActiveGrid(parent->getPositionInformation());
    
    /********
    // Generate id of element
    valuesOfElement[SUMO_ATTR_ID] = generateID(NULL);
    ********/

    // show warning dialogbox and stop check if input parameters are valid
    if (myadditionalParameters->areValuesValid() == false) {
        myadditionalParameters->showWarningMessage();
        return ADDSHAPE_INVALID_ARGUMENTS;
    }

    /********
    // get position in map
    valuesOfElement[SUMO_ATTR_POSITION] = toString(currentPosition);
    ********/

    // Save block value
    valuesOfElement[GNE_ATTR_BLOCK_MOVEMENT] = toString(myEditorParameters->isBlockMovementEnabled());

    // Save block value
    valuesOfElement[GNE_ATTR_BLOCK_SHAPE] = toString(myEditorParameters->isBlockShapeEnabled());

    /***
    // Create additional
    if (GNEShapeHandler::buildShape(myViewNet, true, myActualShapeType, valuesOfElement)) {
        return ADDADDITIONAL_SUCCESS;
    } else {
        return ADDADDITIONAL_INVALID_ARGUMENTS;
    }

    ******/
    return ADDSHAPE_INVALID_ARGUMENTS;
}


long
GNEPolygonFrame::onCmdSelectShape(FXObject*, FXSelector, void*) {
    // Flag to save if name is valid
    bool additionalNameCorrect = false;
    // set parameters of additional, if it's correct
    for (auto i : GNEAttributeCarrier::allowedShapeTags()) {
        if (toString(i) == myShapeMatchBox->getText().text()) {
            myShapeMatchBox->setTextColor(FXRGB(0, 0, 0));
            myadditionalParameters->show();
            myEditorParameters->show();
            setParametersOfShape(i);
            additionalNameCorrect = true;
        }
    }
    // if additional name isn't correct, hidde all
    if (additionalNameCorrect == false) {
        myActualShapeType = SUMO_TAG_NOTHING;
        myShapeMatchBox->setTextColor(FXRGB(255, 0, 0));
        myadditionalParameters->hide();
        myEditorParameters->hide();
    }
    return 1;
}


void
GNEPolygonFrame::show() {
    // Show frame
    GNEFrame::show();
}


void
GNEPolygonFrame::setParametersOfShape(SumoXMLTag actualShapeType) {
    // Set new actualShapeType
    myActualShapeType = actualShapeType;
    // Clear internal attributes
    myadditionalParameters->clearAttributes();
    // Obtain attributes of actual myActualShapeType
    std::vector<std::pair <SumoXMLAttr, std::string> > attrs = GNEAttributeCarrier::allowedAttributes(myActualShapeType);
    // Iterate over attributes of myActualShapeType
    for (std::vector<std::pair <SumoXMLAttr, std::string> >::iterator i = attrs.begin(); i != attrs.end(); i++) {
        if (!GNEAttributeCarrier::isUnique(myActualShapeType, i->first)) {
            myadditionalParameters->addAttribute(myActualShapeType, i->first);
        }
    }
    // if there are parmeters, show and Recalc groupBox
    if (myadditionalParameters->getNumberOfAddedAttributes() > 0) {
        myadditionalParameters->showShapeParameters();
    } else {
        myadditionalParameters->hideShapeParameters();
    }
}


// ---------------------------------------------------------------------------
// GNEPolygonFrame::ShapeAttributeSingle - methods
// ---------------------------------------------------------------------------

GNEPolygonFrame::ShapeAttributeSingle::ShapeAttributeSingle(FXComposite* parent) :
    FXHorizontalFrame(parent, GUIDesignAuxiliarHorizontalFrame),
    myShapeTag(SUMO_TAG_NOTHING),
    myShapeAttr(SUMO_ATTR_NOTHING) {
    // Create visual elements
    myLabel = new FXLabel(this, "name", 0, GUIDesignLabelAttribute);
    myTextFieldInt = new FXTextField(this, GUIDesignTextFieldNCol, this, MID_GNE_MODE_ADDITIONAL_CHANGEPARAMETER_TEXT, GUIDesignTextFieldInt);
    myTextFieldReal = new FXTextField(this, GUIDesignTextFieldNCol, this, MID_GNE_MODE_ADDITIONAL_CHANGEPARAMETER_TEXT, GUIDesignTextFieldReal);
    myTextFieldStrings = new FXTextField(this, GUIDesignTextFieldNCol, this, MID_GNE_MODE_ADDITIONAL_CHANGEPARAMETER_TEXT, GUIDesignTextField);
    myBoolCheckButton = new FXCheckButton(this, "Disabled", this, MID_GNE_MODE_ADDITIONAL_CHANGEPARAMETER_BOOL, GUIDesignCheckButtonAttribute);
    // Hide elements
    hideParameter();
}


GNEPolygonFrame::ShapeAttributeSingle::~ShapeAttributeSingle() {}


void
GNEPolygonFrame::ShapeAttributeSingle::showParameter(SumoXMLTag additionalTag, SumoXMLAttr additionalAttr, std::string value) {
    myShapeTag = additionalTag;
    myShapeAttr = additionalAttr;
    myInvalidValue = "";
    myLabel->setText(toString(myShapeAttr).c_str());
    myLabel->show();
    myTextFieldStrings->setTextColor(FXRGB(0, 0, 0));
    myTextFieldStrings->setText(value.c_str());
    myTextFieldStrings->show();
    show();
}


void
GNEPolygonFrame::ShapeAttributeSingle::showParameter(SumoXMLTag additionalTag, SumoXMLAttr additionalAttr, int value) {
    myShapeTag = additionalTag;
    myShapeAttr = additionalAttr;
    myInvalidValue = "";
    myLabel->setText(toString(myShapeAttr).c_str());
    myLabel->show();
    myTextFieldInt->setTextColor(FXRGB(0, 0, 0));
    myTextFieldInt->setText(toString(value).c_str());
    myTextFieldInt->show();
    show();
}


void
GNEPolygonFrame::ShapeAttributeSingle::showParameter(SumoXMLTag additionalTag, SumoXMLAttr additionalAttr, double value) {
    myShapeTag = additionalTag;
    myShapeAttr = additionalAttr;
    myInvalidValue = "";
    myLabel->setText(toString(myShapeAttr).c_str());
    myLabel->show();
    myTextFieldReal->setTextColor(FXRGB(0, 0, 0));
    myTextFieldReal->setText(toString(value).c_str());
    myTextFieldReal->show();
    show();
}


void
GNEPolygonFrame::ShapeAttributeSingle::showParameter(SumoXMLTag additionalTag, SumoXMLAttr additionalAttr, bool value) {
    myShapeTag = additionalTag;
    myShapeAttr = additionalAttr;
    myInvalidValue = "";
    myLabel->setText(toString(myShapeAttr).c_str());
    myLabel->show();
    if (value) {
        myBoolCheckButton->setCheck(true);
        myBoolCheckButton->setText("true");
    } else {
        myBoolCheckButton->setCheck(false);
        myBoolCheckButton->setText("false");
    }
    myBoolCheckButton->show();
    show();
}


void
GNEPolygonFrame::ShapeAttributeSingle::hideParameter() {
    myShapeTag = SUMO_TAG_NOTHING;
    myShapeAttr = SUMO_ATTR_NOTHING;
    myLabel->hide();
    myTextFieldInt->hide();
    myTextFieldReal->hide();
    myTextFieldStrings->hide();
    myBoolCheckButton->hide();
    hide();
}


SumoXMLTag
GNEPolygonFrame::ShapeAttributeSingle::getTag() const {
    return myShapeTag;
}


SumoXMLAttr
GNEPolygonFrame::ShapeAttributeSingle::getAttr() const {
    return myShapeAttr;
}


std::string
GNEPolygonFrame::ShapeAttributeSingle::getValue() const {
    if (GNEAttributeCarrier::isBool(myShapeTag, myShapeAttr)) {
        return (myBoolCheckButton->getCheck() == 1) ? "true" : "false";
    } else if (GNEAttributeCarrier::isInt(myShapeTag, myShapeAttr)) {
        return myTextFieldInt->getText().text();
    } else if (GNEAttributeCarrier::isFloat(myShapeTag, myShapeAttr) || GNEAttributeCarrier::isTime(myShapeTag, myShapeAttr)) {
        return myTextFieldReal->getText().text();
    } else {
        return myTextFieldStrings->getText().text();
    }
}


const std::string&
GNEPolygonFrame::ShapeAttributeSingle::isAttributeValid() const {
    return myInvalidValue;
}


long
GNEPolygonFrame::ShapeAttributeSingle::onCmdSetAttribute(FXObject*, FXSelector, void*) {
    // We assume that current value is valid
    myInvalidValue = "";
    // Check if format of current value of myTextField is correct
    if (GNEAttributeCarrier::isInt(myShapeTag, myShapeAttr)) {
        if (GNEAttributeCarrier::canParse<int>(myTextFieldInt->getText().text())) {
            // convert string to int
            int intValue = GNEAttributeCarrier::parse<int>(myTextFieldInt->getText().text());
            // Check if int value must be positive
            if (GNEAttributeCarrier::isPositive(myShapeTag, myShapeAttr) && (intValue < 0)) {
                myInvalidValue = "'" + toString(myShapeAttr) + "' cannot be negative";
            }
        } else {
            myInvalidValue = "'" + toString(myShapeAttr) + "' doesn't have a valid 'int' format";
        }
    } else if (GNEAttributeCarrier::isTime(myShapeTag, myShapeAttr)) {
        // time attributes work as positive doubles
        if (GNEAttributeCarrier::canParse<double>(myTextFieldReal->getText().text())) {
            // convert string to double
            double doubleValue = GNEAttributeCarrier::parse<double>(myTextFieldReal->getText().text());
            // Check if parsed value is negative
            if (doubleValue < 0) {
                myInvalidValue = "'" + toString(myShapeAttr) + "' cannot be negative";
            }
        } else {
            myInvalidValue = "'" + toString(myShapeAttr) + "' doesn't have a valid 'time' format";
        }
    } else if (GNEAttributeCarrier::isFloat(myShapeTag, myShapeAttr)) {
        if (GNEAttributeCarrier::canParse<double>(myTextFieldReal->getText().text())) {
            // convert string to double
            double doubleValue = GNEAttributeCarrier::parse<double>(myTextFieldReal->getText().text());
            // Check if double value must be positive
            if (GNEAttributeCarrier::isPositive(myShapeTag, myShapeAttr) && (doubleValue < 0)) {
                myInvalidValue = "'" + toString(myShapeAttr) + "' cannot be negative";
                // check if double value is a probability
            } else if (GNEAttributeCarrier::isProbability(myShapeTag, myShapeAttr) && ((doubleValue < 0) || doubleValue > 1)) {
                myInvalidValue = "'" + toString(myShapeAttr) + "' takes only values between 0 and 1";
            }
        } else {
            myInvalidValue = "'" + toString(myShapeAttr) + "' doesn't have a valid 'float' format";
        }
    } else if (GNEAttributeCarrier::isFilename(myShapeTag, myShapeAttr)) {
        // check if filename format is valid
        if (GNEAttributeCarrier::isValidFilename(myTextFieldStrings->getText().text()) == false) {
            myInvalidValue = "input contains invalid characters for a filename";
        }
    }
    // change color of text field depending of myCurrentValueValid
    if (myInvalidValue.size() == 0) {
        myTextFieldInt->setTextColor(FXRGB(0, 0, 0));
        myTextFieldInt->killFocus();
        myTextFieldReal->setTextColor(FXRGB(0, 0, 0));
        myTextFieldReal->killFocus();
        myTextFieldStrings->setTextColor(FXRGB(0, 0, 0));
        myTextFieldStrings->killFocus();
    } else {
        // IF value of TextField isn't valid, change their color to Red
        myTextFieldInt->setTextColor(FXRGB(255, 0, 0));
        myTextFieldReal->setTextColor(FXRGB(255, 0, 0));
        myTextFieldStrings->setTextColor(FXRGB(255, 0, 0));
    }
    // Update aditional frame
    update();
    return 1;
}


long
GNEPolygonFrame::ShapeAttributeSingle::onCmdSetBooleanAttribute(FXObject*, FXSelector, void*) {
    if (myBoolCheckButton->getCheck()) {
        myBoolCheckButton->setText("true");
    } else {
        myBoolCheckButton->setText("false");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// GNEPolygonFrame::NeteditAttributes- methods
// ---------------------------------------------------------------------------

GNEPolygonFrame::ShapeAttributes::ShapeAttributes(GNEViewNet* viewNet, FXComposite* parent) :
    FXGroupBox(parent, "Internal attributes", GUIDesignGroupBoxFrame),
    myViewNet(viewNet),
    myIndexParameter(0),
    myIndexParameterList(0),
    maxNumberOfParameters(GNEAttributeCarrier::getHigherNumberOfAttributes()),
    maxNumberOfListParameters(2) {

    // Create single parameters
    for (int i = 0; i < maxNumberOfParameters; i++) {
        myVectorOfsingleShapeParameter.push_back(new ShapeAttributeSingle(this));
    }

    // Create help button
    helpShape = new FXButton(this, "Help", 0, this, MID_HELP, GUIDesignButtonRectangular);
}


GNEPolygonFrame::ShapeAttributes::~ShapeAttributes() {
}


void
GNEPolygonFrame::ShapeAttributes::clearAttributes() {
    // Hidde al fields
    for (int i = 0; i < maxNumberOfParameters; i++) {
        myVectorOfsingleShapeParameter.at(i)->hideParameter();
    }

    // Reset indexs
    myIndexParameterList = 0;
    myIndexParameter = 0;
}


void
GNEPolygonFrame::ShapeAttributes::addAttribute(SumoXMLTag additionalTag, SumoXMLAttr ShapeAttributeSingle) {
    // Set current additional
    myShapeTag = additionalTag;

    if (myIndexParameter < maxNumberOfParameters) {
        // Check type of attribute list
        if (GNEAttributeCarrier::isInt(myShapeTag, ShapeAttributeSingle)) {
            myVectorOfsingleShapeParameter.at(myIndexParameter)->showParameter(myShapeTag, ShapeAttributeSingle, GNEAttributeCarrier::getDefaultValue<int>(myShapeTag, ShapeAttributeSingle));
        } else if (GNEAttributeCarrier::isFloat(myShapeTag, ShapeAttributeSingle) || GNEAttributeCarrier::isTime(myShapeTag, ShapeAttributeSingle)) {
            myVectorOfsingleShapeParameter.at(myIndexParameter)->showParameter(myShapeTag, ShapeAttributeSingle, GNEAttributeCarrier::getDefaultValue<double>(myShapeTag, ShapeAttributeSingle));
        } else if (GNEAttributeCarrier::isBool(myShapeTag, ShapeAttributeSingle)) {
            myVectorOfsingleShapeParameter.at(myIndexParameter)->showParameter(myShapeTag, ShapeAttributeSingle, GNEAttributeCarrier::getDefaultValue<bool>(myShapeTag, ShapeAttributeSingle));
        } else if (GNEAttributeCarrier::isString(myShapeTag, ShapeAttributeSingle)) {
            myVectorOfsingleShapeParameter.at(myIndexParameter)->showParameter(myShapeTag, ShapeAttributeSingle, GNEAttributeCarrier::getDefaultValue<std::string>(myShapeTag, ShapeAttributeSingle));
        } else {
            WRITE_WARNING("Attribute '" + toString(ShapeAttributeSingle) + "' doesn't have a defined type. Check definition in GNEAttributeCarrier");
        }
        // Update index parameter
        myIndexParameter++;
    } else {
        WRITE_ERROR("Max number of attributes reached (" + toString(maxNumberOfParameters) + ").");
    }
}


void
GNEPolygonFrame::ShapeAttributes::showShapeParameters() {
    recalc();
    show();
}


void
GNEPolygonFrame::ShapeAttributes::hideShapeParameters() {
    hide();
}


std::map<SumoXMLAttr, std::string>
GNEPolygonFrame::ShapeAttributes::getAttributesAndValues() const {
    std::map<SumoXMLAttr, std::string> values;
    // get standar Parameters
    for (int i = 0; i < myIndexParameter; i++) {
        values[myVectorOfsingleShapeParameter.at(i)->getAttr()] = myVectorOfsingleShapeParameter.at(i)->getValue();
    }
    return values;
}


void
GNEPolygonFrame::ShapeAttributes::showWarningMessage(std::string extra) const {
    std::string errorMessage;
    // iterate over standar parameters
    for (int i = 0; (i < myIndexParameter) && errorMessage.empty(); i++) {
        // Return string with the error if at least one of the parameter isn't valid
        std::string attributeValue = myVectorOfsingleShapeParameter.at(i)->isAttributeValid();
        if (attributeValue.size() != 0) {
            errorMessage = attributeValue;
        }
    }
    // show warning box if input parameters aren't invalid
    if (extra.size() == 0) {
        errorMessage = "Invalid input parameter of " + toString(myShapeTag) + ": " + errorMessage;
    } else {
        errorMessage = "Invalid input parameter of " + toString(myShapeTag) + ": " + extra;
    }

    // set message in status bar
    myViewNet->setStatusBarText(errorMessage);
    // Write Warning in console if we're in testing mode
    if (OptionsCont::getOptions().getBool("gui-testing-debug")) {
        WRITE_WARNING(errorMessage);
    }
}


bool
GNEPolygonFrame::ShapeAttributes::areValuesValid() const {
    // iterate over standar parameters
    for (int i = 0; i < myIndexParameter; i++) {
        // Return false if error message of attriuve isn't empty
        if (myVectorOfsingleShapeParameter.at(i)->isAttributeValid().size() != 0) {
            return false;
        }
    }
    return true;
}


int
GNEPolygonFrame::ShapeAttributes::getNumberOfAddedAttributes() const {
    return (myIndexParameter + myIndexParameterList);
}


long
GNEPolygonFrame::ShapeAttributes::onCmdHelp(FXObject*, FXSelector, void*) {
    // Create help dialog
    FXDialogBox* helpDialog = new FXDialogBox(this, ("Parameters of " + toString(myShapeTag)).c_str(), GUIDesignDialogBox);
    // Create FXTable
    FXTable* myTable = new FXTable(helpDialog, this, MID_TABLE, TABLE_READONLY);
    myTable->setVisibleRows((FXint)(myIndexParameter + myIndexParameterList));
    myTable->setVisibleColumns(3);
    myTable->setTableSize((FXint)(myIndexParameter + myIndexParameterList), 3);
    myTable->setBackColor(FXRGB(255, 255, 255));
    myTable->setColumnText(0, "Name");
    myTable->setColumnText(1, "Value");
    myTable->setColumnText(2, "Definition");
    myTable->getRowHeader()->setWidth(0);
    FXHeader* header = myTable->getColumnHeader();
    header->setItemJustify(0, JUSTIFY_CENTER_X);
    header->setItemSize(0, 120);
    header->setItemJustify(1, JUSTIFY_CENTER_X);
    header->setItemSize(1, 80);
    int maxSizeColumnDefinitions = 0;
    // Iterate over vector of additional parameters
    for (int i = 0; i < myIndexParameter; i++) {
        SumoXMLTag tag = myVectorOfsingleShapeParameter.at(i)->getTag();
        SumoXMLAttr attr = myVectorOfsingleShapeParameter.at(i)->getAttr();
        // Set name of attribute
        myTable->setItem(i, 0, new FXTableItem(toString(attr).c_str()));
        // Set type
        FXTableItem* type = new FXTableItem("");
        if (GNEAttributeCarrier::isInt(tag, attr)) {
            type->setText("int");
        } else if (GNEAttributeCarrier::isFloat(tag, attr)) {
            type->setText("float");
        } else if (GNEAttributeCarrier::isTime(tag, attr)) {
            type->setText("time");
        } else if (GNEAttributeCarrier::isBool(tag, attr)) {
            type->setText("bool");
        } else if (GNEAttributeCarrier::isString(tag, attr)) {
            type->setText("string");
        }
        type->setJustify(FXTableItem::CENTER_X);
        myTable->setItem(i, 1, type);
        // Set definition
        FXTableItem* definition = new FXTableItem(GNEAttributeCarrier::getDefinition(myShapeTag, attr).c_str());
        definition->setJustify(FXTableItem::LEFT);
        myTable->setItem(i, 2, definition);
        if ((int)GNEAttributeCarrier::getDefinition(myShapeTag, attr).size() > maxSizeColumnDefinitions) {
            maxSizeColumnDefinitions = int(GNEAttributeCarrier::getDefinition(myShapeTag, attr).size());
        }
    }
    // Set size of column
    header->setItemJustify(2, JUSTIFY_CENTER_X);
    header->setItemSize(2, maxSizeColumnDefinitions * 6);
    // Button Close
    new FXButton(helpDialog, "OK\t\tclose", GUIIconSubSys::getIcon(ICON_ACCEPT), helpDialog, FXDialogBox::ID_ACCEPT, GUIDesignButtonOK);
    helpDialog->create();
    helpDialog->show();
    return 1;
}

// ---------------------------------------------------------------------------
// GNEPolygonFrame::NeteditAttributes- methods
// ---------------------------------------------------------------------------

GNEPolygonFrame::NeteditAttributes::NeteditAttributes(FXComposite* parent) :
    FXGroupBox(parent, "Netedit attributes", GUIDesignGroupBoxFrame)  {
    // Create Frame for block movement label and checkBox (By default disabled)
    FXHorizontalFrame* blockMovement = new FXHorizontalFrame(this, GUIDesignAuxiliarHorizontalFrame);
    myBlockMovementLabel = new FXLabel(blockMovement, "block movement", 0, GUIDesignLabelAttribute);
    myBlockMovementCheckButton = new FXCheckButton(blockMovement, "false", this, MID_GNE_SET_BLOCKING_MOVEMENT, GUIDesignCheckButtonAttribute);
    myBlockMovementCheckButton->setCheck(false);
    // Create Frame for block shape label and checkBox (By default disabled)
    FXHorizontalFrame* blockShape = new FXHorizontalFrame(this, GUIDesignAuxiliarHorizontalFrame);
    myBlockShapeLabel = new FXLabel(blockShape, "block shape", 0, GUIDesignLabelAttribute);
    myBlockShapeCheckButton = new FXCheckButton(blockShape, "false", this, MID_GNE_SET_BLOCKING_SHAPE, GUIDesignCheckButtonAttribute);
    myBlockShapeCheckButton->setCheck(false);
}


GNEPolygonFrame::NeteditAttributes::~NeteditAttributes() {}


bool
GNEPolygonFrame::NeteditAttributes::isBlockMovementEnabled() const {
    return myBlockMovementCheckButton->getCheck() == 1 ? true : false;
}


bool
GNEPolygonFrame::NeteditAttributes::isBlockShapeEnabled() const {
    return myBlockShapeCheckButton->getCheck() == 1 ? true : false;
}


long
GNEPolygonFrame::NeteditAttributes::onCmdSetBlockMovement(FXObject*, FXSelector, void*) {
    if (myBlockMovementCheckButton->getCheck()) {
        myBlockMovementCheckButton->setText("true");
    } else {
        myBlockMovementCheckButton->setText("false");
    }
    return 1;
}


long
GNEPolygonFrame::NeteditAttributes::onCmdSetBlockShape(FXObject*, FXSelector, void*) {
    if (myBlockShapeCheckButton->getCheck()) {
        myBlockShapeCheckButton->setText("true");
    } else {
        myBlockShapeCheckButton->setText("false");
    }
    return 1;
}


std::string
GNEPolygonFrame::getIdsSelected(const FXList* list) {
    // Obtain Id's of list
    std::string vectorOfIds;
    for (int i = 0; i < list->getNumItems(); i++) {
        if (list->isItemSelected(i)) {
            if (vectorOfIds.size() > 0) {
                vectorOfIds += " ";
            }
            vectorOfIds += (list->getItem(i)->getText()).text();
        }
    }
    return vectorOfIds;
}

/****************************************************************************/
# -*- coding: iso-8859-1 -*-
#
#-------------------------------------------------------------------------------
#
#     This file is part of the Code_Saturne User Interface, element of the
#     Code_Saturne CFD tool.
#
#     Copyright (C) 1998-2009 EDF S.A., France
#
#     contact: saturne-support@edf.fr
#
#     The Code_Saturne User Interface is free software; you can redistribute it
#     and/or modify it under the terms of the GNU General Public License
#     as published by the Free Software Foundation; either version 2 of
#     the License, or (at your option) any later version.
#
#     The Code_Saturne User Interface is distributed in the hope that it will be
#     useful, but WITHOUT ANY WARRANTY; without even the implied warranty
#     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with the Code_Saturne Kernel; if not, write to the
#     Free Software Foundation, Inc.,
#     51 Franklin St, Fifth Floor,
#     Boston, MA  02110-1301  USA
#
#-------------------------------------------------------------------------------

"""
This module defines the Page in which the user defines the path of the treated
case. The Page verify that the case configuration directories is appropriate.

This module contains the following classes:
- IdentityAndPathesView
"""

#-------------------------------------------------------------------------------
# Library modules import
#-------------------------------------------------------------------------------

import os, string
import logging
from PyQt4 import QtGui, QtCore

#-------------------------------------------------------------------------------
# Application modules import
#-------------------------------------------------------------------------------

from Base.Toolbox import GuiParam
from IdentityAndPathesForm import Ui_IdentityAndPathesForm
import Base.QtPage as QtPage
from Pages.IdentityAndPathesModel import IdentityAndPathesModel

#-------------------------------------------------------------------------------
# log config
#-------------------------------------------------------------------------------

logging.basicConfig()
log = logging.getLogger("IdentityAndPathesView")
log.setLevel(GuiParam.DEBUG)

#-------------------------------------------------------------------------------
# Relevant directories label
#-------------------------------------------------------------------------------

sub_dir = ["DATA", "RESU", "SRC", "SCRIPTS"]
meshes_dir = "MESH"
unknown_dir = "????????"

#-------------------------------------------------------------------------------
#
#-------------------------------------------------------------------------------

class IdentityAndPathesView(QtGui.QWidget, Ui_IdentityAndPathesForm):
    """
    Class to open Identity and Pathes Page.
    """
    def __init__(self, parent, case, study):
        """
        Constructor
        """
        QtGui.QWidget.__init__(self, parent)

        Ui_IdentityAndPathesForm.__init__(self)
        self.setupUi(self)

        self.case = case
        self.study = study

        self.path = ['data_path',
                     'resu_path',
                     'user_src_path',
                     'scripts_path',
                     'mesh_path']

        self.mdl = IdentityAndPathesModel(self.case)

        # Create the Page layout.
        self.connect(self.toolButton, QtCore.SIGNAL("pressed()"), self.searchDir)
        self.lineEditCasePath.setReadOnly(True)

        # Initialize the contens of the widgets.
        if not self.case['case_path']:

            # Set case path to its default value
            #
            fic = self.mdl.getXmlFileName()
            if not fic:
                f = os.getcwd()
                if os.path.basename(f) == 'DATA': f = os.path.dirname(f)
                self.mdl.setCasePath(f)
            else :
                file_dir = os.path.split(fic)[0]
                if file_dir:
                    self.mdl.setCasePath(os.path.split(file_dir)[0])
                    if not os.path.basename(file_dir) == 'DATA':

                        title = self.tr("WARNING")
                        msg   = self.tr("Warning: the xml file must be in directory "\
                                        "DATA of the case.")
                        QtGui.QMessageBox.warning(self, title, msg)
                        self.mdl.setCasePath(file_dir)
                else:
                    self.mdl.setCasePath(os.path.split(os.getcwd())[0])

        self.case_path = self.mdl.getCasePath()
        self.lineEditCasePath.setText(self.case_path)
        self.getAbsolutePath()


    def updateId(self, case_path):
        """
        Update Study and Case names in the StudyId bar.
        """
        case_name  = os.path.basename(case_path)
        self.spath = os.path.split(case_path)[0]
        study_name = os.path.basename(self.spath)

        self.mdl.setId(case_name, study_name)
        fic = self.mdl.getXmlFileName()
        self.study.set(study=study_name, case=case_name, filename=fic)


    def searchDir(self):
        """
        Open a File Dialog in order to search the case directory.
        """
        path_case = self.mdl.getCasePath()

        title    = self.tr("Select directory")
        default  = path_case
        options  = QtGui.QFileDialog.ShowDirsOnly # | QtGui.QFileDialog.DontResolveSymlinks
        dir_name = QtGui.QFileDialog.getExistingDirectory(self, title, default, options)
        dir_name = str(dir_name)

        if dir_name:
            self.case_path = dir_name
            self.lineEditCasePath.setText(self.case_path)
            self.getAbsolutePath()
            self.updateId(dir_name)


    def getAbsolutePath(self):
        """
        Get absolute path for the case sub-directories and the meshes.
        """
        self.case_path = str(self.lineEditCasePath.text())
        self.lineEditCasePath.setText(self.case_path)
        case_dir = os.path.abspath(self.case_path)
        self.case_path = case_dir

        line_name = ['Data', 'Results', 'UserSrc', 'Scripts']

        if os.path.isdir(case_dir) :
            self.mdl.setCasePath(case_dir)
            self.updateId(case_dir)

            msg = [self.tr("Warning: the DATA sub-directory DATA is required."),
                   self.tr("Warning: the RESU sub-directory RESU is required."),
                   self.tr("Warning: the SRC sub-directory SRC is required."),
                   self.tr("Warning: the SCRIPTS sub-directory SCRIPTS is required.")]

            msgError = self.tr("Associated sub-directory not found")

            for i in range(0,4) :
                if sub_dir[i] in os.listdir(case_dir):
                    self.mdl.setPath(self.path[i], os.path.abspath(case_dir + '/' + sub_dir[i]))
                    line = getattr(self, "lineEdit"+line_name[i])  # line is self.lineEditXXX
                    line.setText(QtCore.QString(sub_dir[i]))
                    line.setStatusTip("")
                    self.mdl.setRelevantSubdir("yes", sub_dir[i])
                else:
                    self.mdl.setPath(self.path[i], "")
                    line = getattr(self, "lineEdit"+line_name[i])  # line is self.lineEditXXX
                    line.setText(msgError)
                    line.setStatusTip(msg[i])
                    self.mdl.setRelevantSubdir("no", sub_dir[i])

            if os.path.isdir(self.spath) and meshes_dir in os.listdir(self.spath):
                self.mdl.setPath(self.path[4], os.path.abspath(self.spath + '/' + meshes_dir))
                self.lineEditMeshPath.setText(QtCore.QString(meshes_dir))
                self.setStatusTip("")
                self.mdl.setRelevantSubdir("yes", self.path[4])
            else:
                self.mdl.setPath(self.path[4], "")
                self.lineEditMeshPath.setText(msgError)
                msg = self.tr("Warning: the MESH directory "\
                              "is required, at the level of the case directory.")
                self.lineEditMeshPath.setStatusTip(msg)
                self.mdl.setRelevantSubdir("no", self.path[4])

        else:
            for i in range(0,4) :
                line = getattr(self, "lineEdit"+line_name[i])  # line is self.lineEditXXX
                line.setText(unknown_dir)
            self.lineEditMeshPath.setText(unknown_dir)

            msg = self.tr("Warning: the given directory does not exist.")
            self.lineEditMeshPath.setStatusTip(msg)
            self.mdl.setRelevantSubdir("no", '')


    def tr(self, text):
        """
        Translation
        """
        return text

#-------------------------------------------------------------------------------
# Testing part
#-------------------------------------------------------------------------------


if __name__ == "__main__":
    pass


#-------------------------------------------------------------------------------
# End
#-------------------------------------------------------------------------------
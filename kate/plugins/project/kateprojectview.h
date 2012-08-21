/*  This file is part of the Kate project.
 *
 *  Copyright (C) 2010 Christoph Cullmann <cullmann@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef KATE_PROJECT_VIEW_H
#define KATE_PROJECT_VIEW_H

#include "kateproject.h"

#include <QTreeView>

class KateProjectPluginView;

/**
 * Class representing a view of a project.
 * A tree like view of project content.
 */
class KateProjectView : public QTreeView
{
  Q_OBJECT

  public:
    /**
     * construct project view for given project
     * @param pluginView our plugin view
     * @param project project this view is for
     */
    KateProjectView (KateProjectPluginView *pluginView, KateProject *project);

    /**
     * deconstruct project
     */
    ~KateProjectView ();

    /**
     * our project.
     * @return project
     */
    KateProject *project () const
    {
      return m_project;
    }

    /**
     * Select given file in the view.
     * @param file select this file in the view, will be shown if invisible
     */
    void selectFile (const QString &file);

  private Q_SLOTS:
    /**
     * item got clicked, do stuff, like open document
     * @param index model index of clicked item
     */
    void slotClicked (const QModelIndex &index);

    /**
     * Triggered on model changes.
     * This includes the files list, itemForFile mapping!
     */
    void slotModelChanged ();

  protected:
    /**
     * Create matching context menu.
     * @param event context menu event
     */
    void contextMenuEvent (QContextMenuEvent *event);

  private:
    /**
     * our plugin view
     */
    KateProjectPluginView *m_pluginView;

    /**
     * our project
     */
    KateProject *m_project;
};

#endif

// kate: space-indent on; indent-width 2; replace-tabs on;

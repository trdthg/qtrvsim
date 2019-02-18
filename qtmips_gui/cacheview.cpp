// SPDX-License-Identifier: GPL-2.0+
/*******************************************************************************
 * QtMips - MIPS 32-bit Architecture Subset Simulator
 *
 * Implemented to support following courses:
 *
 *   B35APO - Computer Architectures
 *   https://cw.fel.cvut.cz/wiki/courses/b35apo
 *
 *   B4M35PAP - Advanced Computer Architectures
 *   https://cw.fel.cvut.cz/wiki/courses/b4m35pap/start
 *
 * Copyright (c) 2017-2019 Karel Koci<cynerd@email.cz>
 * Copyright (c) 2019      Pavel Pisa <pisa@cmp.felk.cvut.cz>
 *
 * Faculty of Electrical Engineering (http://www.fel.cvut.cz)
 * Czech Technical University        (http://www.cvut.cz/)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/

#include "cacheview.h"
#include "fontsize.h"
#include <cmath>

//////////////////////
#define ROW_HEIGHT 14
#define VD_WIDTH 10
#define DATA_WIDTH 72
#define PENW 1
#define LETTERW 7
//////////////////////

#include <iostream>
using namespace std;

CacheAddressBlock::CacheAddressBlock(const machine::Cache *cache, unsigned width) {
    s_row = cache->config().sets() > 1 ? sqrt(cache->config().sets()) : 0;
    this->width = width;
    s_col = cache->config().blocks() > 1 ? sqrt(cache->config().blocks()) : 0;
    s_tag = 30 - s_row - s_col; // 32 bits - 2 unused  and then every bit used for different index
    this->width = width;

    tag = 0;
    row = 0;
    col = 0;

    connect(cache, SIGNAL(cache_update(uint,uint,bool,bool,std::uint32_t,const std::uint32_t*)), this, SLOT(cache_update(uint,uint,bool,bool,std::uint32_t,const std::uint32_t*)));
}

QRectF CacheAddressBlock::boundingRect() const {
    return QRectF(0, 0, width, 40);
}

void CacheAddressBlock::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QFont fnt;
    fnt.setPointSize(7);
    painter->setFont(fnt);

    unsigned wpos = 5;
    // Part used for tag (we expect that tag is always used)
    unsigned wid = s_tag == 0 ? 0 : ((s_tag / 4) + 1);
    unsigned tag_center = wpos + wid*LETTERW/2 + 1;
    QRectF rect(wpos, 16, wid*LETTERW + 2, ROW_HEIGHT);
    painter->drawRect(rect);
    painter->drawText(rect, Qt::AlignCenter, QString("%1").arg(tag, wid, 16, QChar('0')));
    wpos += wid*LETTERW + 2;
    // Part used for set
    unsigned row_center = wpos;
    if (s_row > 0) {
        wid = s_row == 0 ? 0 : ((s_row / 4) + 1);
        row_center += wid*LETTERW/2 + 1;
        rect = QRectF(wpos, 16, wid*LETTERW + 2, ROW_HEIGHT);
        painter->drawRect(rect);
        painter->drawText(rect, Qt::AlignCenter, QString("%1").arg(row, wid, 16, QChar('0')));
        wpos += wid*LETTERW + 2;
    }
    // Part used for block
    unsigned col_center = wpos;
    if (s_col > 0) {
        wid = s_col == 0 ? 0 : ((s_col / 4) + 1);
        col_center += wid*LETTERW/2 + 1;
        rect = QRectF(wpos, 16, wid*LETTERW + 2, ROW_HEIGHT);
        painter->drawRect(rect);
        painter->drawText(rect, Qt::AlignCenter, QString("%1").arg(col, wid, 16, QChar('0')));
        wpos += wid*LETTERW + 2;
    }
    // Part used for two lowers bits
    painter->setBrush(QBrush(QColor(Qt::gray)));
    painter->drawRect(wpos, 16, LETTERW + 2, ROW_HEIGHT);
    painter->setBrush(QBrush(QColor(Qt::black)));
    wpos += LETTERW + 2;

    // Pain address label
    painter->drawText(QRectF(5, 0, wpos - 5, 14), Qt::AlignCenter, "Address");

    QPen p;
    p.setWidth(2);
    painter->setPen(p);

    // Tag line
    painter->drawLine(-8, 40, -8, 33);
    painter->drawLine(-8, 33, tag_center, 33);
    painter->drawLine(tag_center, 33, tag_center, 30);

    // set line
    if (s_row > 0) {
        painter->drawLine(-4, 40, row_center, 40);
        painter->drawLine(row_center, 40, row_center, 30);
    }

    // block line
    if (s_col > 0) {
        painter->drawLine(width - 16, 40, col_center, 40);
        painter->drawLine(col_center, 40, col_center, 30);
    }
}

void CacheAddressBlock::cache_update(unsigned associat, unsigned set, bool valid, bool dirty, std::uint32_t tag, const std::uint32_t *data) {
    this->tag = tag;
    this->row = 0; // TODO we are missing this information
    this->col = set;
    update();
}


CacheViewBlock::CacheViewBlock(const machine::Cache *cache, unsigned block , bool last) : QGraphicsObject(nullptr) {
    islast = last;
    this->block = block;
    rows = cache->config().sets();
    columns = cache->config().blocks();
    curr_row = 0;

    QFont font;
    font.setPointSize(FontSize::SIZE7);

    validity = new QGraphicsSimpleTextItem*[rows];
    if (cache->config().write_policy() == machine::MachineConfigCache::WP_BACK)
        dirty = new QGraphicsSimpleTextItem*[rows];
    else
        dirty = nullptr;
    tag = new QGraphicsSimpleTextItem*[rows];
    data = new QGraphicsSimpleTextItem**[rows];
    int row_y = 1;
    for (unsigned i = 0; i < rows; i++) {
        int row_x = 2;
        validity[i] = new QGraphicsSimpleTextItem("0", this);
        validity[i]->setPos(row_x, row_y);
        validity[i]->setFont(font);
        row_x += VD_WIDTH;
        if (dirty) {
            dirty[i] = new QGraphicsSimpleTextItem(this);
            dirty[i]->setPos(row_x, row_y);
            dirty[i]->setFont(font);
            row_x += VD_WIDTH;
        }
        tag[i] = new QGraphicsSimpleTextItem(this);
        tag[i]->setPos(row_x, row_y);
        tag[i]->setFont(font);
        row_x += DATA_WIDTH;

        data[i] = new QGraphicsSimpleTextItem*[columns];
        for (unsigned y = 0; y < columns; y++) {
            data[i][y] = new QGraphicsSimpleTextItem(this);
            data[i][y]->setPos(row_x, row_y);
            data[i][y]->setFont(font);
            row_x += DATA_WIDTH;
        }

        row_y += ROW_HEIGHT;
    }

    unsigned wd = 1;
    QGraphicsSimpleTextItem *l_validity = new QGraphicsSimpleTextItem("V", this);
    l_validity->setFont(font);
    QRectF box = l_validity->boundingRect();
    l_validity->setPos(wd + (VD_WIDTH - box.width())/2, -1 - box.height());
    wd += VD_WIDTH;
    if (cache->config().write_policy() == machine::MachineConfigCache::WP_BACK) {
        QGraphicsSimpleTextItem *l_dirty = new QGraphicsSimpleTextItem("D", this);
        l_dirty->setFont(font);
        box = l_dirty->boundingRect();
        l_dirty->setPos(wd + (VD_WIDTH - box.width())/2, -1 - box.height());
        wd += VD_WIDTH;
    }
    QGraphicsSimpleTextItem *l_tag = new QGraphicsSimpleTextItem("Tag", this);
    l_tag->setFont(font);
    box = l_tag->boundingRect();
    l_tag->setPos(wd + (DATA_WIDTH - box.width())/2 , -1 - box.height());
    wd += DATA_WIDTH;
    QGraphicsSimpleTextItem *l_data = new QGraphicsSimpleTextItem("Data", this);
    l_data->setFont(font);
    box = l_data->boundingRect();
    l_data->setPos(wd + (columns*DATA_WIDTH - box.width())/2 , -1 - box.height());

    connect(cache, SIGNAL(cache_update(uint,uint,bool,bool,std::uint32_t,const std::uint32_t*)), this, SLOT(cache_update(uint,uint,bool,bool,std::uint32_t,const std::uint32_t*)));
}

CacheViewBlock::~CacheViewBlock() {
    delete[] validity;
    if (dirty != nullptr)
        delete[] dirty;
    delete[] tag;
    for (unsigned y = 0; y < rows; y++)
        delete[] data[y];
    delete data;
}

QRectF CacheViewBlock::boundingRect() const  {
    return QRectF(
        -PENW / 2 - 11,
        -PENW / 2 - 16,
        VD_WIDTH + (dirty ? VD_WIDTH : 0) + DATA_WIDTH*(columns+1) + PENW + 12 + (columns > 1 ? 7 : 0),
        ROW_HEIGHT*rows + PENW + 50
    );
}

void CacheViewBlock::paint(QPainter *painter, const QStyleOptionGraphicsItem *option __attribute__((unused)), QWidget *widget __attribute__((unused)))  {
    // Draw horizontal lines
    for (unsigned i = 0; i <= rows; i++)
        painter->drawLine(0, i * ROW_HEIGHT, VD_WIDTH + (dirty ? VD_WIDTH : 0) + DATA_WIDTH*(columns + 1), i * ROW_HEIGHT);
    // Draw vertical lines
    painter->drawLine(0, 0, 0, rows*ROW_HEIGHT);
    int c_width = VD_WIDTH;
    painter->drawLine(c_width, 0, c_width, rows*ROW_HEIGHT);
    if (dirty) {
        c_width += VD_WIDTH;
        painter->drawLine(c_width, 0, c_width, rows*ROW_HEIGHT);
    }
    c_width += DATA_WIDTH;
    painter->drawLine(c_width, 0, c_width, rows*ROW_HEIGHT);
    for (unsigned i = 0; i < columns; i++) {
        c_width += DATA_WIDTH;
        painter->drawLine(c_width, 0, c_width, rows*ROW_HEIGHT);
    }

    QPen p_wide, p;
    p.setWidth(1);
    p_wide.setWidth(2);

    painter->setPen(p);

    // Tag compare
    unsigned allright = (dirty ? 2 : 1)*VD_WIDTH + DATA_WIDTH*(columns  + 1);
    unsigned bottom = ROW_HEIGHT*rows;
    unsigned tag_center = (dirty ? 2 : 1)*VD_WIDTH + DATA_WIDTH/2;
    painter->drawEllipse(QPointF(tag_center, bottom + 15), 5, 5);
    painter->drawText(QRectF(tag_center - 5, bottom + 9.5, 10, 10), Qt::AlignCenter, "=");
    painter->setPen(p_wide);
    painter->drawLine(tag_center, bottom, tag_center, bottom + 10);
    painter->setPen(p);

    // And
    painter->drawLine(tag_center + 10, bottom + 25, tag_center + 10, bottom + 35);
    painter->drawLine(tag_center + 10, bottom + 25, tag_center + 15, bottom + 25);
    painter->drawLine(tag_center + 10, bottom + 35, tag_center + 15, bottom + 35);
    painter->drawArc(tag_center + 10, bottom + 25, 10, 10, 270 * 16, 180 * 16);

    // Connection from and to right
    painter->drawLine(tag_center + 20, bottom + 30, allright, bottom + 30);
    // Connection from valid to and
    painter->drawLine(VD_WIDTH/2, bottom, VD_WIDTH/2, bottom + 32);
    painter->drawLine(VD_WIDTH/2, bottom + 32, tag_center + 10, bottom + 32);
    // Connection  from tag compare to and
    painter->drawLine(tag_center, bottom + 20, tag_center, bottom + 28);
    painter->drawLine(tag_center, bottom + 28, tag_center + 10, bottom + 28);

    unsigned data_start = (dirty ? 2 : 1)*VD_WIDTH + DATA_WIDTH;
    if (columns > 1) {
        // Output mutex
        const QPointF poly[] = {
            QPointF(data_start, bottom + 10),
            QPointF(data_start + columns*DATA_WIDTH , bottom + 10),
            QPointF(data_start + columns*DATA_WIDTH - 10, bottom + 20),
            QPointF(data_start + 10, bottom + 20)
        };
        painter->drawPolygon(poly, sizeof(poly) / sizeof(QPointF));
        unsigned data_center = data_start + DATA_WIDTH*columns/2;
        painter->setPen(p_wide);
        painter->drawLine(data_center, bottom + 20, data_center, bottom + 25);
        painter->drawLine(data_center, bottom + 25, allright, bottom + 25);
        for (unsigned i = 0; i < columns; i++) {
            unsigned xpos = data_start + i*DATA_WIDTH + DATA_WIDTH/2;
            painter->drawLine(xpos, bottom, xpos, bottom + 10);
        }

        // Mutex source
        painter->drawLine(allright + 5, -16, allright + 5, bottom + 15);
        painter->drawLine(allright + 5, bottom + 15, data_start + columns*DATA_WIDTH - 4, bottom + 15);
        if (!islast)
            painter->drawLine(allright + 5, bottom + 15, allright + 5, bottom + 40);
    } else {
        // Wire with data to right
        painter->setPen(p_wide);
        painter->drawLine(data_start + DATA_WIDTH/2, bottom, data_start + DATA_WIDTH/2, bottom + 25);
        painter->drawLine(data_start + DATA_WIDTH/2, bottom + 25, allright, bottom + 25);
    }

    // Connection with tag
    painter->setPen(p_wide);
    painter->drawLine(-9, -16, -9, bottom + 15);
    painter->drawLine(-9, bottom + 15, tag_center - 5, bottom + 15);
    if (!islast)
        painter->drawLine(-9, bottom + 15, -9, bottom + 40);

    // Connection with row
    if (rows > 1) {
        unsigned selected = ROW_HEIGHT*curr_row + ROW_HEIGHT/2;
        painter->drawLine(-5, -16, -5, islast ? selected : bottom + 40);
        painter->drawLine(-5, selected, 0, selected);
    }
}

void CacheViewBlock::cache_update(unsigned associat, unsigned set, bool valid, bool dirty, std::uint32_t tag, const std::uint32_t *data) {
    if (associat != block)
        return; // Ignore blocks that are not us
    validity[set]->setText(valid ? "1" : "0");
    if (this->dirty)
        this->dirty[set]->setText(valid ? (dirty ? "1" : "0") : "");
    // TODO calculate correct size of tag
    this->tag[set]->setText(valid ? QString("0x") + QString("%1").arg(tag, 8, 16, QChar('0')).toUpper() : "");
    for (unsigned i = 0; i < columns; i++)
        this->data[set][i]->setText(valid ? QString("0x") + QString("%1").arg(data[i], 8, 16, QChar('0')).toUpper() : "");

    curr_row = set;
    update();
}


CacheViewScene::CacheViewScene(const machine::Cache *cache) {
    associativity = cache->config().associativity();
    block = new CacheViewBlock*[associativity];
    int offset = 0;
    for (unsigned i = 0; i < associativity; i++) {
        block[i] = new CacheViewBlock(cache, i, i >= (associativity - 1));
        addItem(block[i]);
        block[i]->setPos(1, offset);
        offset += block[i]->boundingRect().height();
    }
    ablock = new CacheAddressBlock(cache, block[0]->boundingRect().width());
    addItem(ablock);
    ablock->setPos(0, -ablock->boundingRect().height() -16);
}

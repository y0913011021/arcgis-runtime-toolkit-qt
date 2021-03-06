
/*******************************************************************************
 *  Copyright 2012-2018 Esri
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ******************************************************************************/

#include "LayerListModel.h"
#include "Map.h"
#include "MapQuickView.h"
#include "Scene.h"
#include "SceneQuickView.h"
#include "TimeAware.h"
#include "TimeValue.h"

#include "TimeSliderController.h"
#include "ToolManager.h"

#include <cstring>

using namespace Esri::ArcGISRuntime;

namespace Esri
{
namespace ArcGISRuntime
{
namespace Toolkit
{

/*!
 \internal
 */
TimeExtent unionTimeExtent(const TimeExtent& timeExtent,
                           const TimeExtent& otherTimeExtent)
{
  if (otherTimeExtent.isEmpty())
    return timeExtent;

  auto startTime = timeExtent.startTime() < otherTimeExtent.startTime() ? timeExtent.startTime() : otherTimeExtent.startTime();
  auto endTime = timeExtent.endTime() > otherTimeExtent.endTime() ? timeExtent.endTime() : otherTimeExtent.endTime();

  return TimeExtent(startTime, endTime);
}

/*!
 \internal
 */
TimeUnit toTimeUnit(double milisecondsRange)
{
  constexpr double daysPerYear = 365.0;
  constexpr double millisecondsPerDay = 86400000.0;
  constexpr double millisecondsPerHour = 3600000.0;
  constexpr double millisecondsPerMinute = 60000.0;

  if (milisecondsRange < millisecondsPerMinute)
    return TimeUnit::Seconds;
  else if (milisecondsRange < millisecondsPerHour)
    return TimeUnit::Minutes;
  else if (milisecondsRange < millisecondsPerDay)
    return TimeUnit::Hours;
  else if (milisecondsRange < (millisecondsPerDay * daysPerYear))
    return TimeUnit::Days;
  else if (milisecondsRange > (millisecondsPerDay * daysPerYear * 100))
    return TimeUnit::Centuries;
  else
    return TimeUnit::Years;
}

/*!
 \internal
 */
double toMilliseconds(const TimeValue& timeValue)
{
  constexpr double millisecondsPerDay = 86400000.0;
  constexpr double daysPerCentury = 36500.0;
  constexpr double daysPerDecade = 3650.0;
  constexpr double daysPerYear = 365.0;
  constexpr int mothsPerYear = 12;
  constexpr double millisecondsPerHour = 3600000.0;
  constexpr double millisecondsPerMinute = 60000.0;
  constexpr double millisecondsPerSecond = 1000.0;

  switch (timeValue.unit())
  {
  case TimeUnit::Centuries:
      return timeValue.duration() * millisecondsPerDay * daysPerCentury;
  case TimeUnit::Decades:
      return timeValue.duration() * millisecondsPerDay * daysPerDecade;
  case TimeUnit::Years:
      return timeValue.duration() * millisecondsPerDay * daysPerYear;
  case TimeUnit::Months:
      return timeValue.duration() * (daysPerYear / mothsPerYear) * millisecondsPerDay;
  case TimeUnit::Weeks:
      return timeValue.duration() * 604800000;
  case TimeUnit::Days:
      return timeValue.duration() * millisecondsPerDay;
  case TimeUnit::Hours:
      return timeValue.duration() * millisecondsPerHour;
  case TimeUnit::Minutes:
      return timeValue.duration() * millisecondsPerMinute;
  case TimeUnit::Seconds:
      return timeValue.duration() * millisecondsPerSecond;
  case TimeUnit::Milliseconds:
      return timeValue.duration();
  default:
      return timeValue.duration();
  }
}

/*!
 \internal
 */
bool operator >(const TimeValue& timeValue, const TimeValue& otherTimeValue)
{
  if (timeValue.unit() == otherTimeValue.unit())
      return timeValue.duration() > otherTimeValue.duration();
  else
    return toMilliseconds(timeValue) > toMilliseconds(otherTimeValue);
}

/*!
 \internal
 */
bool operator == (const TimeExtent& timeExtent, const TimeExtent& otherTimeExtent)
{
  return timeExtent.startTime() == otherTimeExtent.startTime()
      && timeExtent.endTime() == otherTimeExtent.endTime();
}

/*!
  \class Esri::ArcGISRuntime::Toolkit::TimeSliderController
  \inmodule ArcGISQtToolkit
  \ingroup ToolTimeSlider
  \since Esri::ArcGISRuntime 100.3

  \brief The controller for the Time Slider tool.

  The controller presents the temporal range of the data as a number of steps.
  These steps allow the temporal extent to be set and animated by stepping through
  the range.

  \note The controller will be automatically created by a TimeSlider
  so you do not need to create this type.

  \sa {Time Slider Tool}
 */

/*!
   \brief The constructor that accepts an optional \a parent object.
 */
TimeSliderController::TimeSliderController(QObject* parent):
  AbstractTool(parent)
{
  ToolManager::instance().addTool(this);
}

/*!
   \brief The destructor.
 */
TimeSliderController::~TimeSliderController()
{
}

/*!
   \brief The name of this tool \c "TimeSlider".
 */
QString TimeSliderController::toolName() const
{
  return "TimeSlider";
}

QObject* TimeSliderController::geoView() const
{
  if (m_mapView)
    return m_mapView;
  else if (m_sceneView)
    return m_sceneView;

  return nullptr;
}

/*!
  \brief Sets the GeoView for this tool to \a geoView.

  The view should be either a MapQuickView or a SceneQuickView.

  \note This property will be provided by the TimeSlider so you do not need
  to set this.
 */
void TimeSliderController::setGeoView(QObject* geoView)
{
  if (std::strcmp(geoView->metaObject()->className(), MapQuickView::staticMetaObject.className()) == 0)
  {
    m_mapView = reinterpret_cast<MapQuickView*>(geoView);
    m_sceneView = nullptr;

    if (m_mapView)
    {
      connect(m_mapView, &MapQuickView::mapChanged, this, &TimeSliderController::onMapChanged);
      onMapChanged();
    }
  }
  else if (std::strcmp(geoView->metaObject()->className(), SceneQuickView::staticMetaObject.className()) == 0)
  {
    m_sceneView = reinterpret_cast<SceneQuickView*>(geoView);
    m_mapView = nullptr;

    if (m_sceneView)
    {
      connect(m_sceneView, &SceneQuickView::sceneChanged, this, &TimeSliderController::onSceneChanged);
      onSceneChanged();
    }
  }

  calculateStepPositions();
  emit currentTimeExtentChanged();
}

/*!
 \internal
 */
void TimeSliderController::initializeTimeProperties()
{
  if (!m_operationalLayers)
    return;

  // Get all the layers that are visible and are participating in time-based filtering
  QList<TimeAware*> timeAwareLayers;
  for (int i = 0 ; i < m_operationalLayers->rowCount(); i++)
  {
    auto layer = m_operationalLayers->at(i);
    if (!layer)
      continue;

    auto timeAwareLayer = dynamic_cast<TimeAware*>(layer);
    if (!timeAwareLayer)
      continue;

    if (layer->loadStatus() != LoadStatus::Loaded && layer->loadStatus() != LoadStatus::FailedToLoad)
    {
      connect(layer, &Layer::doneLoading, this, &TimeSliderController::onOperationalLayersChanged);
      continue;
    }

    if (!timeAwareLayer->isTimeFilteringEnabled())
      continue;

    if (!layer->isVisible())
      continue;

    timeAwareLayers.append(timeAwareLayer);
  }

  if (timeAwareLayers.empty())
    return;

  TimeValue timeStepInterval;
  for (auto timeAware : qAsConst(timeAwareLayers))
  {
    if (!timeAware)
      continue;

    setFullTimeExtent(unionTimeExtent(timeAware->fullTimeExtent(), m_fullTimeExtent));

    const auto timeAwareInterval = timeAware->timeInterval();
    if (timeStepInterval.isEmpty())
      timeStepInterval = timeAwareInterval;
    else if (timeAwareInterval > timeStepInterval)
      timeStepInterval = timeAwareInterval;
  }

  const auto start = m_fullTimeExtent.startTime().toMSecsSinceEpoch();
  const auto end = m_fullTimeExtent.endTime().toMSecsSinceEpoch();
  const auto range = end - start;

  if (timeStepInterval.isEmpty())
  {
    TimeUnit estimatedUnit = toTimeUnit(range);
    timeStepInterval = TimeValue(1.0, estimatedUnit);
  }

  m_intervalMS = toMilliseconds(timeStepInterval);

  setNumberOfSteps((range / m_intervalMS) + 1);

  calculateStepPositions();

  setStepTimes();

  emit currentTimeExtentChanged();
}

/*!
 \internal
 */
void TimeSliderController::setNumberOfSteps(int numberOfSteps)
{
  if (numberOfSteps == m_numberOfSteps)
    return;

  m_numberOfSteps = numberOfSteps;
  emit numberOfStepsChanged();
}

void TimeSliderController::setStepTimes()
{
  m_stepTimes.clear();

  for (auto i = 0; i < m_numberOfSteps; ++i)
    m_stepTimes.push_back( m_fullTimeExtent.startTime().addMSecs(i * m_intervalMS) );

  emit stepTimesChanged();
}

/*!
 \brief Returns the full time extent of the data in the current geoView.
 */
TimeExtent TimeSliderController::fullTimeExtent() const
{
  return m_fullTimeExtent;
}

/*!
 \brief Returns the start time of the data in the current geoView.
 */
QDateTime TimeSliderController::fullExtentStart() const
{
  return m_fullTimeExtent.startTime();
}

/*!
 \brief Returns the end time of the data in the current geoView.
 */
QDateTime TimeSliderController::fullExtentEnd() const
{
  return m_fullTimeExtent.endTime();
}

/*!
 \internal
 */
void TimeSliderController::setFullTimeExtent(const TimeExtent& fullTimeExtent)
{
  if (fullTimeExtent == m_fullTimeExtent)
    return;

  m_fullTimeExtent = fullTimeExtent;
  emit fullTimeExtentChanged();
}

/*!
 \brief Returns the current time extent of the data in the current geoView.
 */
TimeExtent TimeSliderController::currentTimeExtent() const
{
  const auto geoViewExtent = m_sceneView ? m_sceneView->timeExtent()
                     : m_mapView ? m_mapView->timeExtent()
                                 : m_fullTimeExtent;

  return geoViewExtent.isEmpty() ? m_fullTimeExtent : geoViewExtent;
}

/*!
 \brief Returns the start time of the current temporal extent of the geoView.
 */
QDateTime TimeSliderController::currentExtentStart() const
{
  return currentTimeExtent().startTime();
}

/*!
 \brief Returns the end time of the current temporal extent of the geoView.
 */
QDateTime TimeSliderController::currentExtentEnd() const
{
  return currentTimeExtent().endTime();
}

/*!
 \brief Returns the end step of the current time extent.

 \sa startStep
 \sa numberOfSteps
 */
int TimeSliderController::endStep() const
{
  return m_endStep;
}

QVariantList TimeSliderController::stepTimes() const
{
  return m_stepTimes;
}

/*!
 \brief Sets the start step index of the current time extent to \a intervalIndex.

 \sa numberOfSteps
 */
void TimeSliderController::setStartInterval(int intervalIndex)
{
  if (m_fullTimeExtent.isEmpty())
      return;

  const auto start = m_fullTimeExtent.startTime().toMSecsSinceEpoch();
  const auto newStart = QDateTime::fromMSecsSinceEpoch(start + (intervalIndex * m_intervalMS));

  auto newExtent = TimeExtent(newStart, currentExtentEnd());
  if (m_sceneView)
    m_sceneView->setTimeExtent(newExtent);
  else if (m_mapView)
    m_mapView->setTimeExtent(newExtent);

  calculateStepPositions();
  emit currentTimeExtentChanged();
}

/*!
 \brief Sets the end step index of the current time extent to \a intervalIndex.

 \sa numberOfSteps
 */
void TimeSliderController::setEndInterval(int intervalIndex)
{
  if (m_fullTimeExtent.isEmpty())
    return;

  const auto start = m_fullTimeExtent.startTime().toMSecsSinceEpoch();
  const auto newEnd = QDateTime::fromMSecsSinceEpoch(start + (intervalIndex * m_intervalMS));

  auto newExtent = TimeExtent(currentExtentStart(), newEnd);
  if (m_sceneView)
    m_sceneView->setTimeExtent(newExtent);
  else if (m_mapView)
    m_mapView->setTimeExtent(newExtent);

  calculateStepPositions();
  emit currentTimeExtentChanged();
}

/*!
 \brief Sets the start and end steps of the current time extent to \a startIndex and \a endIndex.

 \sa numberOfSteps
 */
void TimeSliderController::setStartAndEndIntervals(int startIndex, int endIndex)
{
  if (m_fullTimeExtent.isEmpty())
    return;

  const auto start = m_fullTimeExtent.startTime().toMSecsSinceEpoch();
  const auto newStart = QDateTime::fromMSecsSinceEpoch(start + (startIndex * m_intervalMS));
  const auto newEnd = QDateTime::fromMSecsSinceEpoch(start + (endIndex * m_intervalMS));

  auto newExtent = TimeExtent(newStart, newEnd);
  if (m_sceneView)
    m_sceneView->setTimeExtent(newExtent);
  else if (m_mapView)
    m_mapView->setTimeExtent(newExtent);

  calculateStepPositions();
  emit currentTimeExtentChanged();
}

/*!
 \internal
 */
void TimeSliderController::setEndStep(int endStep)
{
  if (m_endStep == endStep)
    return;

  m_endStep = endStep;
  emit endStepChanged();
}

/*!
 \internal
 */
void TimeSliderController::calculateStepPositions()
{
  if (m_fullTimeExtent.isEmpty())
    return;

  const auto fullStartMs = fullExtentStart().toMSecsSinceEpoch();
  m_startStep = (currentExtentStart().toMSecsSinceEpoch() - fullStartMs) / m_intervalMS;
  m_endStep = (currentExtentEnd().toMSecsSinceEpoch() - fullStartMs) / m_intervalMS;

  emit startStepChanged();
  emit endStepChanged();
}

/*!
 \brief Returns the start step of the current time extent.

 \sa endStep
 \sa numberOfSteps
 */
int TimeSliderController::startStep() const
{
  return m_startStep;
}

/*!
 \internal
 */
void TimeSliderController::setStartStep(int startStep)
{
  if (m_startStep == startStep)
    return;

  m_startStep = startStep;
  emit startStepChanged();
}

/*!
 \brief Returns the total number of required steps to cover the full time extent.

 This figure is based on the full temporal range of the data in the geoView
 and the time intervals used by the data.

 \sa endStep
 \sa startStep
 */
int TimeSliderController::numberOfSteps() const
{
  return m_numberOfSteps;
}

/*!
 \internal
 */
void TimeSliderController::onOperationalLayersChanged()
{
  initializeTimeProperties();
}

/*!
 \internal
 */
void TimeSliderController::onMapChanged()
{
  if (!m_mapView->map())
    return;

  m_operationalLayers = m_mapView->map()->operationalLayers();
  connect(m_operationalLayers, &LayerListModel::layerAdded, this, &TimeSliderController::onOperationalLayersChanged);
  connect(m_operationalLayers, &LayerListModel::layerRemoved, this, &TimeSliderController::onOperationalLayersChanged);
  initializeTimeProperties();
}

/*!
 \internal
 */
void TimeSliderController::onSceneChanged()
{
  if (!m_sceneView->arcGISScene())
    return;

  m_operationalLayers = m_sceneView->arcGISScene()->operationalLayers();
  connect(m_operationalLayers, &LayerListModel::layerAdded, this, &TimeSliderController::onOperationalLayersChanged);
  connect(m_operationalLayers, &LayerListModel::layerRemoved, this, &TimeSliderController::onOperationalLayersChanged);
  initializeTimeProperties();
}

/*!
  \fn void TimeSliderController::numberOfStepsChanged()
  \brief Signal emitted when the \l numberOfSteps property changes.
 */

/*!
  \fn void TimeSliderController::fullTimeExtentChanged()
  \brief Signal emitted when the \l fullTimeExtent property changes.
 */

/*!
  \fn void TimeSliderController::currentTimeExtentChanged()
  \brief Signal emitted when the \l currentTimeExtent property changes.
 */

/*!
  \fn void TimeSliderController::startStepChanged()
  \brief Signal emitted when the \l startStep property changes.
 */

/*!
  \fn void TimeSliderController::endStepChanged()
  \brief Signal emitted when the \l endStep property changes.
 */

} // Toolkit
} // ArcGISRuntime
} // Esri

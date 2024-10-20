Haier Climate Select
====================

.. seo::
    :description: Instructions for setting up additional select components for Haier climate devices.
    :image: haier.svg

Select components to support vertical and horizontal airflow directions settings for Haier AC.

.. code-block:: yaml

    # Example configuration entry
    select:
      - platform: haier
        horizontal_airflow:
          name: Haier airflow horizontal
        vertical_airflow:
          name: Haier airflow vertical

Configuration variables:
------------------------

- **haier_id** (**Required**, :ref:`config-id`): The id of Haier climate component
- **horizontal_airflow** (*Optional*): (supported only by hOn) Select component to control horizontal airflow mode (if supported by AC).
  All options from :ref:`Select <config-select>`.
- **vertical_airflow** (*Optional*): (supported only by hOn) Select component to control vertical airflow mode (if supported by AC).
  All options from :ref:`Select <config-select>`.

See Also
--------

- :doc:`Haier Climate </components/climate/haier>`
- :ghedit:`Edit`

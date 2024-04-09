# Modular Heatmaps
C++ implementation of Modular Heatmaps.
- Inspired by Dave Mark's GDC talk https://www.gdcvault.com/play/1025243/Spatial-Knowledge-Representation-through-Modular

# Features
- Heatmaps (Container for influence data and contains functionality to spread influence).
- HeatmapManager (Responsible for keeping heatmaps up-to-date based on user data(influenceComponent).
- InfluenceComponent (Defines which types of influences a user should apply to the heatmaps).
- Workmap implementation (The interface to collect information from the heatmaps).
- InterestCurves (Different types of curves that defines the falloff ratio of influence).

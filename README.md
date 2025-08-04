# Modular Heatmaps
C++ implementation of Modular Heatmaps. Inspired by Dave Mark's GDC talk - [GDC link](https://www.gdcvault.com/play/1025243/Spatial-Knowledge-Representation-through-Modular)

# Goal
The goal is to give AIs the ability to make more advanced decisions and to retrieve data on what's happening in the world. <br/><br/>
**For a more in-depth explanation, check out my portfolio section for [Heatmaps](https://drossen.github.io/portfolio/#influencemaps).**

# Features
- **Heatmaps** (Container for influence data and contains functionality to spread influence).
- **HeatmapManager** (Responsible for keeping heatmaps up-to-date based on user data(influenceComponent).
- **InfluenceComponent** (Defines which types of influences a user should apply to the heatmaps).
- **Workmap implementation** (The interface to collect information from the heatmaps).
- **InterestCurves** (Defines the falloff ratio of influence).



# ESPUI-Timelaps
 lgarnier11<br/>
 v1.1.2 - 25/02/2025<br/>
 <br/>
 Objectif : gestion de l'allumage d'une ampoule (toutes les x secondes), d'une sonde de température, et d'un chauffage.<br/>
 Utilisation synchronisée avec une application timelaps sur une tablette (afin d'allumer la lumière le temps de la prise de vue.)</br>
 <br/>
 Le circuit devient serveur wifi, et présente une page contenant deux onglets :<br/>
 - Mesures      : décompte avant allumage de l'ampoule</br>
                  état de l'ampoule</br>
                  état du chauffage</br>
                  Reset du timer (positionne le timer à 5 secondes avant l'allumage de l'ampoule)</br>
 - Paramétrages : Délais entre les déclenchements</br>
                  Température de déclenchement du chauffage</br>

 Adaptation en c++ du "framework" Arduino de Neodyme (afin d'utiliser vscode...).<br/>
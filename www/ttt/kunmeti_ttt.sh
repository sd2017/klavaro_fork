#/bin/bash

# Farotaj taskoj antaŭ ol procezi dosierojn elŝutitajn el www.transifex.com
# Aldoni la lingvo-kodon en la listo sube, kaj krei dosierujojn kun tiu kodo ĉi tie kaj en ../htdocs.
# Forigi en la dosieroj 11, 21 e 31: <html>, <head>, ..., </div>, </body>, </html>. (Atentu por ne forigi la <title> !!!)
# Aldoni la koncernatan lingvan ligon en xx/03a-lingvoj.htm kaj ../htdocs/index.html

export M4PATH=`pwd`

for SUBD in ca cs de el eo eu en fr gl hr hu lt pa pl pt ru; do

	MAKRO="m4 -D _LINGVO_KODO_=${SUBD} difinoj.m4"

	cd ${SUBD};

	cat ../xx/00-kapo.htm 11-klavaro.htm 02-chefmenuo.htm ../xx/03a-lingvoj.htm 03-krommenuo.htm ../xx/04-piedo.htm > index.html;
	$MAKRO index.html > index.htm4;
	sed -i -e "s/<br>/<br \/>/g" index.htm4 
	sed -i -e "s/<hr>/<hr \/>/g" index.htm4 
	mv index.htm4 index.html;

	cat ../xx/00-kapo.htm 31-kontrib.htm 02-chefmenuo.htm ../xx/03a-lingvoj.htm 03-krommenuo.htm ../xx/04-piedo.htm > contrib.html;
	$MAKRO contrib.html > contrib.htm4;
	sed -i -e "s/<br>/<br \/>/g" contrib.htm4 
	sed -i -e "s/<hr>/<hr \/>/g" contrib.htm4 
	mv contrib.htm4 contrib.html;

	if test ${SUBD} != "eu"; then 
	  cat ../xx/00-kapo.htm 21-helpo.htm   02-chefmenuo.htm ../xx/03a-lingvoj.htm 03-krommenuo.htm ../xx/04-piedo.htm > help.html;
	  $MAKRO help.html > help.htm4;
	  sed -i -e "s/<br>/<br \/>/g" help.htm4 
	  sed -i -e "s/<hr>/<hr \/>/g" help.htm4 
	  mv help.htm4 help.html;
        fi

	mv *.html ../../htdocs/${SUBD};
	cd ../;
done

cd en;
cat ../xx/00-kapo.htm 41-traduko.htm 02-chefmenuo.htm ../xx/03a-lingvoj.htm 03-krommenuo.htm ../xx/04-piedo.htm > translation.html;
m4 -D _LINGVO_KODO_=en difinoj.m4 translation.html > translation.htm4;
sed -i -e "s/<br>/<br \/>/g" translation.htm4 
sed -i -e "s/<hr>/<hr \/>/g" translation.htm4 
mv translation.htm4 ../../htdocs/en/translation.html;
rm translation.html
cd ../;

cd top10;
m4 top10.htm.in > ../../htdocs/top10/top10.htm;
m4 topbot.htm.in > ../../htdocs/top10/topbot.htm;

cd ../;

exit 0


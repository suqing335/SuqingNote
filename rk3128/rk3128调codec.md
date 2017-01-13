
                rk818_ldo2_reg: regulator@5 {
                        reg = <5>;
                        regulator-compatible = "rk818_ldo2";
-               //      regulator-always-on;
+                       regulator-always-on;
                        regulator-boot-on;
                };
有一路供电没有打开，所以声音一直都没出来，
regulator-always-on;//意思是一开机打开，并且为on
